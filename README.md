# NeuralAmpModelerReamp

Don't forget to download the submodules specified in .gitmodules!

```bash
git submodule update --init --recursive
```

## Howto build

```bash
mkdir build && cd build
```

```bash
cmake ..
```

```bash
make
```

## Howto run from the build directory

```bash
./tools/reamp ../testfiles/05-full-metal.nam ../testfiles/first_5_seconds.wav output.wav
```

## Sharp edges

This library uses [Eigen](http://eigen.tuxfamily.org) to do the linear algebra routines that its neural networks require. Since these models hold their parameters as eigen object members, there is a risk with certain compilers and compiler optimizations that their memory is not aligned properly. This can be worked around by providing two preprocessor macros: `EIGEN_MAX_ALIGN_BYTES 0` and `EIGEN_DONT_VECTORIZE`, though this will probably harm performance. See [Structs Having Eigen Members](http://eigen.tuxfamily.org/dox-3.2/group__TopicStructHavingEigenMembers.html) for more information. This is being tracked as [Issue 67](https://github.com/sdatkinson/NeuralAmpModelerCore/issues/67).
