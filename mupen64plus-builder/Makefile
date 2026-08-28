# TrimUI Brick (tg5040) mupen64plus build system
#
# Sources (submodules under src/): core/ui/audio/input/rsp @2.6.0,
# glide64mk2 @ nightly-build (b07cb0b). Toolchain: GCC 16.2.0 baked into the
# docker image. Sysroot: downloaded by the Dockerfile (TrimUI SDK release
# 20231018), cached across builds.
#
# Critical: OPTFLAGS is "-O3 -mcpu=cortex-a53" WITHOUT -flto (LTO breaks
# glide64mk2 at runtime). Baked into docker/build-all.sh.

IMAGE ?= mupen64plus-builder/toolchain:gcc16
REGISTRY_IMAGE ?= docker.io/aradhyac9/mupen64plus-builder-toolchain:gcc16

.PHONY: setup image release publish clean

setup:
	git submodule update --init --recursive
	bash sysroot/download.sh

image:
	bash scripts/build-image.sh $(IMAGE)

release:
	bash scripts/build-release.sh $(IMAGE)

publish:
	bash scripts/publish-image.sh $(REGISTRY_IMAGE)

clean:
	rm -rf dist
