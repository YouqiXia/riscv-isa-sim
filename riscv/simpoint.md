# SimPoint support on Spike

## Overview

SimPoint is a performance analysis tool developed by the Department of Computer Sciences at the University of Wisconsin-
Madison. It is designed to aid researchers and engineers in the evaluation and optimization of computer program performance.

SimPoint works by dividing the instruction stream of a program into distinct phases, referred to as SimPoints. Each 
SimPoint represents a critical point in the program's execution and captures the program's overall behavior. By 
selecting appropriate SimPoints, it is possible to accurately capture the program's performance characteristics while 
minimizing the simulation overhead.

Using SimPoint, programs can be simulated and various performance metrics, such as instruction counts and cache hit 
rates, can be collected. The simulation results can then be analyzed to evaluate the impact of different optimization 
strategies and guide the design and optimization of computer systems.

## Usage

Spike already provides simpoint support and provides the following options:

```bash
                        The following is the parameter of simpoint,
                        simpoint only supports single core now
  --simpoint=<filename> Read a simpoint file to create multiple checkpoints
  --intervals=<num>     The basic blocks executed in each program, breaking
                        the program up into contiguous intervals of size N
                        (for example, 1 million, 10 million, or 100 million instructions)
  --simpoint_roi        Enable the region of interest, spike will trace the phase of program form the 
                        beginning without write simpoint custom csr.
  --simpoint_start      If simpoint_roi is not enabled, spike will trace the phase of program from pc
                        greater than or equal to simpoint_start, the default value is UINT64_MAX
  --maxinsns=<num>      Terminates execution after a number of instructions
  --load=<filename>     Resumes a previously saved snapshot
  --save=<filename>     Saves a snapshot upon exit
  --compress            Load/save a compressed snapshot, saving space but taking more time

```

### Create/Restore Checkpoint

As a basic function of Simpoint, we can use the `--load` and `--save` parameters to create and restore a checkpoint.

> Note: We will use a case in mlpref below. If you want to try it, please make sure that your spike has been compiled 
> correctly and can execute case correctly.

#### Create Checkpoint

In order to create a checkpoint, we need to tell spike when to create a checkpoint, using the `--maxinsns` option to 
tell spike how many instructions to execute before terminating. Once spike terminates, it will create a checkpoint with 
the name given in the `--save` option.

```bash
spike -m256 --isa=RV64GCV_zicntr --varch=vlen:512,elen:64 --save=sp --maxinsns=1000000 anomaly_detection_0721_clang_vector_fsmt.elf_pk 
bbl loader
N_WARMUP 1, N_INFER 10, N_WAV 5, N_SLICE 1
prepare to read labels
read labels
normal_id_01_00000003_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000263_hist_librosa.bin,2,1,2560,512
normal_id_01_00000013_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000253_hist_librosa.bin,2,1,2560,512
normal_id_01_00000023_hist_librosa.bin,2,0,2560,512
normal_id_01_00000033_hist_librosa.bin,2,0,2560,512
normal_id_01_00000043_hist_librosa.bin,2,0,2560,512
normal_id_01_00000053_hist_librosa.bin,2,0,2560,512
normal_id_01_00000063_hist_librosa.bin,2,0,2560,512
NOTE: creating a new regs file: sp.re_regs
NOTE: creating a new boot rom:  sp.bootram
NOTE: creating a new main ram:  sp.mainram
NOTE: creating a new fds file:  sp.fds

*** core0 exit due to reaching maxinsns. ***
```

As the log above shows, by default a checkpoint will create four files:
- sp.bootram: Contains the boot code needed to restore checkpoint.
- sp.mainram: Contains the copies of memory needed to restore checkpoint.
- sp.fds: If pk is used, this file will hold the file descriptor mapping in syscall.
- sp.re_regs: Contains the register information in text form.

#### Restore Checkpoint

When restoring, we need to specify the checkpoint name to be restored through `--load` config, please keep the `-m`, 
`--isa` and other spike config consistent with when saving the checkpoint.

```bash
spike -m256 --isa=RV64GCV_zicntr --varch=vlen:512,elen:64 --load=sp --maxinsns=1000000 anomaly_detection_0721_clang_vector_fsmt.elf_pk 
normal_id_01_00000073_hist_librosa.bin,2,0,2560,512
normal_id_01_00000083_hist_librosa.bin,2,0,2560,512
normal_id_01_00000093_hist_librosa.bin,2,0,2560,512
normal_id_01_00000103_hist_librosa.bin,2,0,2560,512
normal_id_01_00000113_hist_librosa.bin,2,0,2560,512
normal_id_01_00000123_hist_librosa.bin,2,0,2560,512
normal_id_01_00000133_hist_librosa.bin,2,0,2560,512
normal_id_01_00000143_hist_librosa.bin,2,0,2560,512
normal_id_01_00000153_hist_librosa.bin,2,0,2560,512
normal_id_01_00000163_hist_librosa.bin,2,0,2560,512
normal_id_01_00000173_hist_librosa.bin,2,0,2560,512
m-name-th_serialport_initialize-[Initializing...]
m-timestamp-mode-performance
m-lap-cycles-1155148
4 bytes lost due to alignment. To avoid this loss, please make sure the tensor_arena is 16 bytes aligned.
Initialized
m-init-done
m-ready

m-[Expecting 501760 bytes]
m-ready

*** core0 exit due to reaching maxinsns. ***
```

If maxinsns is not set when restoring a checkpoint, then spike will run until the end of the program. If program and spike will
communicate through `HTIF`, you need to specify program when restoring the checkpoint, so that spike can re-acquire the address
of `tohost` and `formhost`.

By default, mainram is stored as raw binary, if the size of mainram is too large, we can also choose to store a 
compressed version. Make sure you have the `zip/unzip` tool installed on host, and then enable the `--compress` option
when `load/save`.

The checkpoint generated by dromajo does not contain the fd mapping in the syscall. When restoring on spike, you may 
create an empty .fds file or get a warning message.

#### Restore Checkpoint on GEM5

GEM5 has added support for the checkpoint generated by spike. Unlike spike, we need to use more parameters when using GEM5

```bash
gem5.fast \
  --debug-flags=ELFinfo \
  /work/home18/yangcheng/repo/pineapple/software/gem5/configs/example/riscv/fs_linux.py \
  --debug_boot \
  --dtb-filename=spike.dtb \
  --bootrom=sp2.bootram \
  --load_mem=sp2.mainram \
  --load_fds=sp2.fds \
  --kernel=anomaly_detection_0721_clang_vector_fsmt.elf_pk \
  --cpu-type=NewCPU \
  --yaml-config=/work/home18/yangcheng/repo/pineapple/software/gem5/src/cpu/picorio/model/config/P600.yaml \
  --mem-size=2GB \
  --ruby

gem5 Simulator System.  http://gem5.org
gem5 is copyrighted software; use the --copyright option for details.

gem5 version 21.2.1.0
gem5 compiled Jul 28 2023 15:41:15
gem5 started Jul 28 2023 16:49:09
gem5 executing on cad20, pid 12984
command line: gem5.fast --debug-flags=ELFinfo /work/home18/yangcheng/repo/pineapple/software/gem5/configs/example/riscv/fs_linux.py --debug_boot --dtb-filename=spike.dtb --bootrom=sp2.bootram --load_mem=sp2.mainram --load_fds=sp2.fds --kernel=anomaly_detection_0721_clang_vector_fsmt.elf_pk --cpu-type=NewCPU --yaml-config=/work/home18/yangcheng/repo/pineapple/software/gem5/src/cpu/picorio/model/config/P600.yaml --mem-size=2GB --ruby

0 1
warn: cntrl.memory is deprecated. The request port for Ruby memory output to the main memory is now called `memory_out_port`
Global frequency set at 1000000000000 ticks per second
warn: No dot file generated. Please install pydot to generate the dot file and pdf.
abs memory range: 80000000, 100000000
build/RISCV_RUNC_FAST/mem/mem_interface.cc:793: warn: DRAM device capacity (8192 Mbytes) does not match the address range assigned (2048 Mbytes)
build/RISCV_RUNC_FAST/sim/kernel_workload.cc:46: info: kernel located at: anomaly_detection_0721_clang_vector_fsmt.elf_pk
[info] -> Successfully Init Logger(Console)
name: tohost, addr = 0x80697008 
name: fromhost, addr = 0x80697000 
build/RISCV_RUNC_FAST/base/statistics.hh:280: warn: One of the stats is a legacy stat. Legacy stat is a stat that does not belong to any statistics::Group. Legacy stat is deprecated.
      0: system.platform.rtc: Real-time clock set to Sun Jan  1 00:00:00 2012
system.platform.terminal: Listening for connections on port 3456
0: system.remote_gdb: listening for remote gdb on port 7000
build/RISCV_RUNC_FAST/mem/coherent_xbar.cc:140: warn: CoherentXBar system.membus has no snooping ports attached!
build/RISCV_RUNC_FAST/arch/riscv/linux/fs_workload.cc:51: info: Loading DTB file: spike.dtb at address 0x87e00000
**** REAL SIMULATION ****
get memfile size 268435456
      0: system.cpu: Load mem file(sp2.mainram) into ram Successfully
      0: system.cpu: Load fds file(sp2.fds) into syscall proxy Successfully
      0: system.cpu: New CPU BootROM Address 0x10000, Kenerl Entry 0x80000000
build/RISCV_RUNC_FAST/sim/simulate.cc:199: info: Entering event queue @ 0.  Starting simulation...
bLeaving Debug Mode, Dpc: 800030d8, Priv: 3
Simpoint Resumed, Minstret: 44, Mcycle: 44
build/RISCV_RUNC_FAST/mem/ruby/system/Sequencer.cc:587: warn: Replacement policy updates recently became the responsibility of SLICC state machines. Make sure to setMRU() near callbacks in .sm files!
bl loader
N_WARMUP 1, N_INFER 10, N_WAV 5, N_SLICE 1
prepare to read labels
read labels
normal_id_01_00000003_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000263_hist_librosa.bin,2,1,2560,512
normal_id_01_00000013_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000253_hist_librosa.bin,2,1,2560,512
normal_id_01_00000023_hist_librosa.bin,2,0,2560,512
normal_id_01_00000033_hist_librosa.bin,2,0,2560,512
normal_id_01_00000043_hist_librosa.bin,2,0,2560,512
normal_id_01_00000053_hist_librosa.bin,2,0,2560,512
normal_id_01_00000063_hist_librosa.bin,2,0,2560,512
normal_id_01_00000073_hist_librosa.bin,2,0,2560,512
normal_id_01_00000083_hist_librosa.bin,2,0,2560,512
normal_id_01_00000093_hist_librosa.bin,2,0,2560,512
normal_id_01_00000103_hist_librosa.bin,2,0,2560,512
normal_id_01_00000113_hist_librosa.bin,2,0,2560,512
normal_id_01_00000123_hist_librosa.bin,2,0,2560,512
normal_id_01_00000133_hist_librosa.bin,2,0,2560,512
normal_id_01_00000143_hist_librosa.bin,2,0,2560,512
normal_id_01_00000153_hist_librosa.bin,2,0,2560,512
normal_id_01_00000163_hist_librosa.bin,2,0,2560,512
normal_id_01_00000173_hist_librosa.bin,2,0,2560,512
m-name-th_serialport_initialize-[Initializing...]
m-timestamp-mode-performance
m-lap-cycles-1722884

.....

```

The key options are as follows:
- --debug_boot: This option sets gem5 to boot in debug mode, which is required by the bootrom.
- --dtb-filename: This option sets the dtb file, please use the same dtb as spike to ensure that the addresses read and written by the program are consistent.
- --bootrom: This option specifies the boot code used to restore checkpoint registers.
- --load_mem: This option specifies the mainram used to restore checkpoint memory
- --load_fds: This option specifies the file used to restore fd mapping in syscall

If program and GEM5 will communicate through `HTIF`, you need to specify `--kernel` when restoring the checkpoint, so that GEM5 can re-acquire the address
of `tohost` and `formhost`.

### Create SimPoint

#### Create spike_simpoint.bb

In order to generate simpoint, we need to run elf to generate the trace file first.

```bash
spike -m256 --isa=RV64GCV_zicntr --varch=vlen:512,elen:64 --simpoint_roi --intervals=1000000 anomaly_detection_0721_clang_vector_fsmt.elf_pk 
bbl loader
N_WARMUP 1, N_INFER 10, N_WAV 5, N_SLICE 1
prepare to read labels
read labels
normal_id_01_00000003_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000263_hist_librosa.bin,2,1,2560,512
normal_id_01_00000013_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000253_hist_librosa.bin,2,1,2560,512
normal_id_01_00000023_hist_librosa.bin,2,0,2560,512
normal_id_01_00000033_hist_librosa.bin,2,0,2560,512
normal_id_01_00000043_hist_librosa.bin,2,0,2560,512
normal_id_01_00000053_hist_librosa.bin,2,0,2560,512
normal_id_01_00000063_hist_librosa.bin,2,0,2560,512
normal_id_01_00000073_hist_librosa.bin,2,0,2560,512
normal_id_01_00000083_hist_librosa.bin,2,0,2560,512
normal_id_01_00000093_hist_librosa.bin,2,0,2560,512
normal_id_01_00000103_hist_librosa.bin,2,0,2560,512
normal_id_01_00000113_hist_librosa.bin,2,0,2560,512
normal_id_01_00000123_hist_librosa.bin,2,0,2560,512
normal_id_01_00000133_hist_librosa.bin,2,0,2560,512
normal_id_01_00000143_hist_librosa.bin,2,0,2560,512
normal_id_01_00000153_hist_librosa.bin,2,0,2560,512
normal_id_01_00000163_hist_librosa.bin,2,0,2560,512
normal_id_01_00000173_hist_librosa.bin,2,0,2560,512
m-name-th_serialport_initialize-[Initializing...]
m-timestamp-mode-performance
m-lap-cycles-1159910
4 bytes lost due to alignment. To avoid this loss, please make sure the tensor_arena is 16 bytes aligned.
Initialized
m-init-done
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-6865927
m-lap-cycles-9565531
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-13105009
m-lap-cycles-15804376
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-19312114
m-lap-cycles-22011481
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-25518983
m-lap-cycles-28218350
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-31726260
m-lap-cycles-34425627
m-infer-done
m-results-[11.766]
m-ready
Timestamp cycles: 1159910
Timestamp cycles: 6865927
Timestamp cycles: 9565531
Timestamp cycles: 13105009
Timestamp cycles: 15804376
Timestamp cycles: 19312114
Timestamp cycles: 22011481
Timestamp cycles: 25518983
Timestamp cycles: 28218350
Timestamp cycles: 31726260
Timestamp cycles: 34425627

No label for normal or anomalous element, abort...



***************************************
For every 10 inference(s) and 1 slice(s)[640 long], on 5 wav(s), 
        >>> AUC: -1.000000
        >>> Performance: 269941 cycles (mean)
***************************************
*** core0 PASSED exit (tohost=0x1) *** 

write basic block trace to spike_simpoint.bb
core0 total instructions : 34618475
```

The `--simpoint_roi` option enables the region of interest, and spike will trace the phase of program form the beginning.
Another alternative is `--simpoint_start` option, spike will trace phase of program form pc >= simpoint_start, for example,
for program running in the pk environment, you can start from 0xffffffc000000000 to skip the pk initialization phase.

```bash
spike -m256 --isa=RV64GCV_zicntr --varch=vlen:512,elen:64 --simpoint_start=0xffffffc000000000 --intervals=1000000 anomaly_detection_0721_clang_vector_fsmt.elf_pk
```

The `--intervals` option sets the size of the basic block. If you don`t know what a basic block is, please refer to the papers
of simpoint. 

As shown in the log, after running the program, spike will generate a `spike_simpoint.bb` file, which we will use to 
generate simpoints and weights files. but before that you need to install simpoint tool first.

#### Build SimPoint Tool

```bash
git clone "ssh://yourname@gerrit01.ours.com:29418/rivai-ic/simpoint" && scp -p -P 29418 yourname@gerrit01.ours.com:hooks/commit-msg "simpoint/.git/hooks/"
cd simpoint
make Simpoint
```

The simpoint tool will be compiled in the build directory.

#### Create simpoints and weights

How to select simpoint region depends on your restrictions, but usual parameter:

```bash
simpoint -maxK 30 -saveSimpoints simpoints -saveSimpointWeights weights -loadFVFile spike_simpoint.bb
```

The simpoints file should have  the list of checkpoints and the location. For example, this contents means that there 
are 6 checkpoints, and the first starts at 19simpoint_size. The 2nd starts at 1simpoint_size... All the checkpoints 
have the same size of simpoint_size. The simpoints size is equal to `--intervals`.

```text
19 0
1 1
0 2
22 3
6 4
11 5
```

#### Create SimPoint Checkpoints

We can use spike to automatically generate the checkpoint selected by the simpoint tool based on this file.

```bash
spike -m256 --isa=RV64GCV_zicntr --varch=vlen:512,elen:64 --simpoint_roi --intervals=1000000 --simpoint=simpoints anomaly_detection_0721_clang_vector_fsmt.elf_pk 

simpoint 2 start at 551

simpoint 1 start at 1000000

simpoint 4 start at 6000000

simpoint 5 start at 11000000

simpoint 0 start at 19000000

simpoint 3 start at 22000000
NOTE: creating a new regs file: sp2.re_regs
NOTE: creating a new boot rom:  sp2.bootram
NOTE: creating a new main ram:  sp2.mainram
NOTE: creating a new fds file:  sp2.fds
bbl loader
N_WARMUP 1, N_INFER 10, N_WAV 5, N_SLICE 1
prepare to read labels
read labels
normal_id_01_00000003_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000263_hist_librosa.bin,2,1,2560,512
normal_id_01_00000013_hist_librosa.bin,2,0,2560,512
anomaly_id_04_00000253_hist_librosa.bin,2,1,2560,512
normal_id_01_00000023_hist_librosa.bin,2,0,2560,512
normal_id_01_00000033_hist_librosa.bin,2,0,2560,512
normal_id_01_00000043_hist_librosa.bin,2,0,2560,512
normal_id_01_00000053_hist_librosa.bin,2,0,2560,512
normal_id_01_00000063_hist_librosa.bin,2,0,2560,512
NOTE: creating a new regs file: sp1.re_regs
NOTE: creating a new boot rom:  sp1.bootram
NOTE: creating a new main ram:  sp1.mainram
NOTE: creating a new fds file:  sp1.fds
normal_id_01_00000073_hist_librosa.bin,2,0,2560,512
normal_id_01_00000083_hist_librosa.bin,2,0,2560,512
normal_id_01_00000093_hist_librosa.bin,2,0,2560,512
normal_id_01_00000103_hist_librosa.bin,2,0,2560,512
normal_id_01_00000113_hist_librosa.bin,2,0,2560,512
normal_id_01_00000123_hist_librosa.bin,2,0,2560,512
normal_id_01_00000133_hist_librosa.bin,2,0,2560,512
normal_id_01_00000143_hist_librosa.bin,2,0,2560,512
normal_id_01_00000153_hist_librosa.bin,2,0,2560,512
normal_id_01_00000163_hist_librosa.bin,2,0,2560,512
normal_id_01_00000173_hist_librosa.bin,2,0,2560,512
m-name-th_serialport_initialize-[Initializing...]
m-timestamp-mode-performance
m-lap-cycles-1159910
4 bytes lost due to alignment. To avoid this loss, please make sure the tensor_arena is 16 bytes aligned.
Initialized
m-init-done
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
NOTE: creating a new regs file: sp4.re_regs
NOTE: creating a new boot rom:  sp4.bootram
NOTE: creating a new main ram:  sp4.mainram
NOTE: creating a new fds file:  sp4.fds
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-6865927
m-lap-cycles-9565531
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
NOTE: creating a new regs file: sp5.re_regs
NOTE: creating a new boot rom:  sp5.bootram
NOTE: creating a new main ram:  sp5.mainram
NOTE: creating a new fds file:  sp5.fds
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-13105009
m-lap-cycles-15804376
m-infer-done
m-results-[11.766]
m-ready

m-[Expecting 501760 bytes]
m-ready
m-load-file-"normal_id_01_00000003_hist_librosa.bin"
NOTE: creating a new regs file: sp0.re_regs
NOTE: creating a new boot rom:  sp0.bootram
NOTE: creating a new main ram:  sp0.mainram
NOTE: creating a new fds file:  sp0.fds
m-load-done
m-ready
m-warmup-start-1
n_warmup index: 0
m-warmup-done
m-infer-pos: 0
m-infer-start-10
m-lap-cycles-19312114
NOTE: creating a new regs file: sp3.re_regs
NOTE: creating a new boot rom:  sp3.bootram
NOTE: creating a new main ram:  sp3.mainram
NOTE: creating a new fds file:  sp3.fds

*** core0 exit because all checkpoints create finished. ***
```

> Note: Keep the simpoint_start/simpoint_roi and intervals options consistent with those when generating the spike_simpoint.bb


