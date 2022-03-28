# Spack Tips for Amanzi

To build Amanzi, and for many spack workflows, you only need a few simple spack commands.  This document highlights the commands we find most useful, gives examples of how to use them, and what information they provide.

## Spack Commands

Here's a quick list of the commands we use most often:

  * Setup and Build Environment 
    * spack compiler find
    * spack external find
  * Build and Install
    * spack install <package>
    * spack spec <package>
  * ...
  
  
## Spack Environements 

Spack environments allow you to create specific containers of spack modules for your project. 
See the spack complete guide on thee [environments](https://spack.readthedocs.io/en/latest/environments.html)

Create the environment: 
```shell
spack env create amanzi
```

Load the environment:
```shell
spack env activate amanzi 
```

Unload/quit an enviroment: 
```shell 
spack env disactivate 
# Shortcut (same as previous): 
despacktivate 
```

From inside the environment you can now install spackages as you would do using the `spack install` command. 
They will be added to the current environment and loaded directly when the environment is activated. 

## Spack Setup

Here we need to develop instructions on how to setup the compilers,
mpi, cmake, and other tools that we want for our build environment.
I think there are three cases we need to discuss here.

  * Let spack build everything (including, compilers, mpi, cmake)
  * Tell spack where local cmake, gcc, mpi are located (without modules)
  * Tell spack where local cmake, gcc, mpi are located with modules
 
A condensed/simple version of these instructions would probably go in
the README.md. But here I'm hoping we can include a bit more detail
and provide instructions on how to verify that the setup is correct.

### Let Spack build and manage it all

Here we'd be looking at spack building gcc, cmake, openmpi, and openblas

ParFlow provides this guideance, it's a good start for us to think about.
Building GCC will take considerable amount of time.

```shell
spack install gcc@10.3.0
```

Add the compiler to the set of compilers Spack can use:

```shell
spack load gcc@10.3.0
spack compiler find
spack compilers
```



### Configure Spack to use locally installed software

Here we'd be looking at first make sure that cmake, gcc, openmpi are in your path and then running the compiler and externals setup

```
  spack compiler find
```

```
  spack externals find
```

Finally, we'd need to pick and configure blas/lapack within packages/yaml file.  I wonder if we need an entire section on blas/lapack, particularly if we end up supporting mkl.

### Configure Spack to use modules to manage locally installed software

Here we would still use 
```
  spack compiler find
```

```
  spack externals find
```

But then we would update by hand the entries associated with the modules we want spack to be able to use.   Not sure how to make this easiest on the user.


## Working with Spack

Here we look at the commands used most often, e.g., install, spec,
info, find etc.






