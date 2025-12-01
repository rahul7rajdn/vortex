#!/bin/bash
#SBATCH -Jvortex-ci                              # Job name
#SBATCH -N1 --cpus-per-task=16                	 # Number of nodes and CPUs per node required
#SBATCH --mem-per-cpu=4G                         # Memory per core
#SBATCH -t 02:00:00                              # Duration of the job (Ex: 2 hours)
#SBATCH -p rg-nextgen-hpc                        # Partition Name
#SBATCH -o /tools/ci-reports/vortex-ci-test-%j.out   # Combined output and error messages file
#SBATCH -W                                       # Do not exit until the submitted job terminates.

##Add commands here to build and execute
cd $GITHUB_WORKSPACE
echo $GITHUB_WORKSPACE
hostname
pwd
ls
cd $GITHUB_WORKSPACE/miscs/apptainer
chmod +x run_apptainer.sh
apptainer exec --bind ../../../vortex:/home/vortex  --bind  /projects/tools/x86_64/common-tools/vortex-tools:/home/tools /projects/tools/x86_64/containers/vortex_micro25.sif /home/vortex/miscs/apptainer/run_vortex.sh
pwd
echo $HOME
hostname

