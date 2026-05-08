#include <iostream>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <ctime>
#include <vortex.h>
#include <mpi.h>
#include "common.h"

#define RT_CHECK(_expr)                                         \
   do {                                                         \
     int _ret = _expr;                                          \
     if (0 == _ret)                                             \
       break;                                                   \
     printf("Error: '%s' returned %d!\n", #_expr, (int)_ret);  \
     cleanup();                                                 \
     MPI_Abort(MPI_COMM_WORLD, -1);                             \
   } while (false)

const char* kernel_file = "kernel.vxbin";
uint32_t chunk_size = 1 << 18;
int iterations = 20;

vx_device_h device     = nullptr;
vx_buffer_h buf_local  = nullptr;
vx_buffer_h buf_halo   = nullptr;
vx_buffer_h buf_output = nullptr;
vx_buffer_h krnl_buffer = nullptr;
vx_buffer_h args_buffer = nullptr;

void cleanup() {
  if (device) {
    vx_mem_free(buf_local);
    vx_mem_free(buf_halo);
    vx_mem_free(buf_output);
    vx_mem_free(krnl_buffer);
    vx_mem_free(args_buffer);
    vx_dev_close(device);
  }
}

static double get_time_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void parse_args(int argc, char** argv) {
  int c;
  while ((c = getopt(argc, argv, "n:i:k:h")) != -1) {
    switch (c) {
      case 'n': chunk_size = atoi(optarg); break;
      case 'i': iterations = atoi(optarg); break;
      case 'k': kernel_file = optarg; break;
      case 'h':
        std::cout << "Usage: [-n chunk_floats] [-i iterations] [-k kernel] [-h help]\n";
        exit(0);
      default:
        std::cout << "Usage: [-n chunk_floats] [-i iterations] [-k kernel] [-h help]\n";
        exit(-1);
    }
  }
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  char hostname[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(hostname, &name_len);
  std::cout << "rank = " << rank << ", world_size = " << world_size
            << ", host = " << hostname << "\n";

  if (rank == 0) parse_args(argc, argv);
  MPI_Bcast(&chunk_size, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  MPI_Bcast(&iterations, 1, MPI_INT,      0, MPI_COMM_WORLD);

  int neighbor = (rank + 1) % world_size;

  if (rank == 0) {
    printf("=== MPI Halo Exchange Benchmark (non-blocking) ===\n");
    printf("Ranks: %d  Chunk: %u floats (%.2f MB)  Iterations: %d\n",
           world_size, chunk_size, chunk_size * 4.0 / (1024*1024), iterations);
    printf("Total data per rank: %.2f MB\n",
           chunk_size * 4.0 * iterations / (1024*1024));
    fflush(stdout);
  }

  uint32_t buf_size = chunk_size * sizeof(float);

  // Host buffers
  std::vector<float> h_local(chunk_size);
  std::vector<float> h_halo(chunk_size, 0.0f);
  std::vector<float> h_output(chunk_size, 0.0f);

  for (uint32_t i = 0; i < chunk_size; i++) {
    h_local[i] = (float)(rank + 1);
  }

  // Open Vortex device
  RT_CHECK(vx_dev_open(&device));

  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_READ,  &buf_local));
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_READ,  &buf_halo));
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_WRITE, &buf_output));

  kernel_arg_t kernel_arg = {};
  kernel_arg.count = chunk_size;
  RT_CHECK(vx_mem_address(buf_local,  &kernel_arg.local_addr));
  RT_CHECK(vx_mem_address(buf_halo,   &kernel_arg.halo_addr));
  RT_CHECK(vx_mem_address(buf_output, &kernel_arg.output_addr));

  RT_CHECK(vx_copy_to_dev(buf_local, h_local.data(), 0, buf_size));

  std::cout << "Rank: " << rank << "- Upload kernel binary\n";
  RT_CHECK(vx_upload_kernel_file(device, kernel_file, &krnl_buffer));

  // ── Warmup: one Vortex run before timing starts ──────────────────
  RT_CHECK(vx_upload_bytes(device, &kernel_arg, sizeof(kernel_arg_t), &args_buffer));
  RT_CHECK(vx_start(device, krnl_buffer, args_buffer));
  RT_CHECK(vx_ready_wait(device, VX_MAX_TIMEOUT));

  // ── Synchronize all ranks before starting timed loop ─────────────
  MPI_Barrier(MPI_COMM_WORLD);

  double t_comm_total = 0.0;
  double t_comp_total = 0.0;
  double t_total_start = get_time_ms();

  for (int iter = 0; iter < iterations; iter++) {

    // ── Barrier: ensure both ranks start comm at the same time ──────
    // This is the key fix — both ranks must be ready before measuring
    MPI_Barrier(MPI_COMM_WORLD);

    // ── Step 1: Non-blocking send + receive simultaneously ──────────
    // Both ranks post Irecv FIRST (before Isend) to avoid deadlock
    double t0 = get_time_ms();

    MPI_Request reqs[2];
    MPI_Irecv(h_halo.data(),  chunk_size, MPI_FLOAT, neighbor, iter,
              MPI_COMM_WORLD, &reqs[0]);
    MPI_Isend(h_local.data(), chunk_size, MPI_FLOAT, neighbor, iter,
              MPI_COMM_WORLD, &reqs[1]);
    MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);

    double t1 = get_time_ms();
    t_comm_total += (t1 - t0);

    // ── Step 2: Upload received halo to Vortex device ───────────────
    RT_CHECK(vx_copy_to_dev(buf_halo, h_halo.data(), 0, buf_size));

    // ── Step 3: Upload kernel args ───────────────────────────────────
    RT_CHECK(vx_upload_bytes(device, &kernel_arg, sizeof(kernel_arg_t), &args_buffer));

    // ── Step 4: Run Vortex kernel ────────────────────────────────────
    double t2 = get_time_ms();
    RT_CHECK(vx_start(device, krnl_buffer, args_buffer));
    RT_CHECK(vx_ready_wait(device, VX_MAX_TIMEOUT));
    double t3 = get_time_ms();
    t_comp_total += (t3 - t2);

    // ── Step 5: Copy result back on last iteration ───────────────────
    if (iter == iterations - 1) {
      RT_CHECK(vx_copy_from_dev(h_output.data(), buf_output, 0, buf_size));
    }
  }

  double t_total = get_time_ms() - t_total_start;

  // Final barrier before printing
  MPI_Barrier(MPI_COMM_WORLD);

  // Validation: output[i] = local[i] + halo[i] = (rank+1) + (neighbor+1) = 3.0
  float expected = 3.0f;
  int errors = 0;
  for (uint32_t i = 0; i < chunk_size; i++) {
    if (std::fabs(h_output[i] - expected) > 1e-4f) {
      if (errors < 10)
        printf("*** error: [%d] expected=%f actual=%f\n", i, expected, h_output[i]);
      errors++;
    }
  }

  double bw_mb = (buf_size * (double)iterations) / (t_comm_total * 1e3);

  printf("\n--- Rank %d (%s) ---\n", rank, hostname);
  printf("  Comm:    %.1f ms total  %.2f ms/iter  %.1f MB/s\n",
         t_comm_total, t_comm_total / iterations, bw_mb);
  printf("  Compute: %.1f ms total  %.2f ms/iter\n",
         t_comp_total, t_comp_total / iterations);
  printf("  Total:   %.1f ms\n", t_total);
  printf("  Result:  %s (%d errors)\n",
         errors == 0 ? "PASSED!" : "FAILED!", errors);
  fflush(stdout);

  cleanup();
  MPI_Finalize();
  return 0;
}