# KasKas

<!-- CI status -->
[![ci](https://github.com/s-t-a-n/KasKas/actions/workflows/ci.yml/badge.svg)](https://github.com/s-t-a-n/KasKas/actions?query=workflow=ci)
[![last commit](https://badgen.net/github/last-commit/s-t-a-n/KasKas)](https://GitHub.com/s-t-a-n/KasKas/commit/)
[![license](https://img.shields.io/github/license/s-t-a-n/KasKas.svg)](https://github.com/s-t-a-n/KasKas/blob/main/LICENSE)


<!-- short description -->
Embedded C++ Application for a running a little greenhouse.

## Why

Plants are chaotic creatures that run on their own code without much outside effort, but what makes them tick? I thought
it would be cool to make a controlled environment and try to grow some tomatoplants. A weekend project turned
into a month long project as I've found this box to be an excellent platform for scopecreep.

|                                       Chamber                                       |                                       Control                                       |                                       Outside                                       |
:-----------------------------------------------------------------------------------:|:-----------------------------------------------------------------------------------:|:-----------------------------------------------------------------------------------:
| ![](https://github.com/s-t-a-n/kaskas/blob/develop/doc/kaskas_chamber.png?raw=true) | ![](https://github.com/s-t-a-n/kaskas/blob/develop/doc/kaskas_control.png?raw=true) | ![](https://github.com/s-t-a-n/kaskas/blob/develop/doc/kaskas_outside.png?raw=true) |

Note: Some people think wood is a bad choice of material to make a controlbox containing current driving MOSFETS out of.
That's not a bad point. This, however, is an experimental setup (an EE friend dubbed frankenstein's nightmare box) aimed
at running
for only a couple of weeks. Although the electronics look and are rather messy, every current carrying line is fused for
rated currents and the various thermal fuses and software thermal
run away protection ought to
stave off most fires. The breadboards and the linear regulator are leftovers, they merely connect some low current for
sensors and drive the controller. I am
currently whipping up a little pcb in KiCAD so that V2 will be a greenhouse that fits in your backpocket!

Powered by [Spine](https://github.com/s-t-a-n/Spine).

## Coding philosophy

- The code in this repo is meant to be experimental. I try to adhere to good standards for obvious reasons but in order
  not to get bogged down in perfection, lenience is applied where feasible. Code that ends up generic enough to be reuse
  in other embedded projects are migrated to s-t-a-n/Spine.

## Design philosophy

- keep all configurable parameters inside the main.cpp so any arbitrary settings end up in a single place
- keep the various components as modular as possible
- this is not work, this is _fun_

## Schematic overview

![](https://github.com/s-t-a-n/kaskas/blob/develop/doc/kaskas_schema.png?raw=true)

## How to run

1. Get PlatformIO.
2. Run `pio run` in the root of repository to compile
3. Run `pio test -e unittest` in the root of repository to run the unittests

## Contribute

Feel free to open an issue if you like the project, have an idea or would like to collaborate on something else!
