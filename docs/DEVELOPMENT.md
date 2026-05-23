# Development Notes

## MSYS2 UCRT64 packages

```bash
pacman -S --needed git \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-libiio \
  mingw-w64-ucrt-x86_64-libad9361-iio \
  mingw-w64-ucrt-x86_64-fftw
```

## Build

```bash
./tools/build_native_ucrt64.sh
```

## Git workflow

```bash
git status
git add .
git commit -m "Describe the change"
git push
```
