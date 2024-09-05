# KasKas

<!-- CI status -->
[![ci](https://github.com/s-t-a-n/KasKas/workflows/ci.yml/badge.svg)](https://github.com/s-t-a-n/KasKas/actions?query=workflow=ci)
[![last commit](https://badgen.net/github/last-commit/s-t-a-n/KasKas)](https://GitHub.com/s-t-a-n/KasKas/commit/)
[![license](https://img.shields.io/github/license/s-t-a-n/KasKas.svg)](https://github.com/s-t-a-n/KasKas/blob/master/LICENSE)


<!-- short description -->
Embedded C++ Application for a running a little greenhouse.

| Chamber | Control | Outside |
:-------------:|:--------------:|:--------------:
| ![](https://github.com/s-t-a-n/kaskas/blob/master/doc/kaskas_chamber.jpg?raw=true) | ![](https://github.com/s-t-a-n/kaskas/blob/master/doc/kaskas_control.png?raw=true) | ![](https://github.com/s-t-a-n/kaskas/blob/master/doc/kaskas_outside.png?raw=true) |

Note: No, they are tomatoplants.

Note: Some people think wood is a bad choice of material to make a controlbox containing current dirving MOSFETS out of. I couldnt agree more. This is an experimental setup aimed at running for only a couple of weeks. Although the electronics look and are rather messy, every current carrying line is fused for rated currents and various thermal fuses and thermal run away protection ought to keep the box 'safe enough'. The goal of this box is not even to grow tomatoes per se, but rather to experiment and see if interesting data (and feedback) can be achieved using just some cheap probes.

Powered by [Spine](https://github.com/s-t-a-n/Spine).

## Why

Plants are chaotic creatures that run on their own code without any outside effort, but what makes them tick? I thought it would be be a cool idea to make a controlled environment and try to grow some tomatoplants. A weekend project turned into a month long project as I've found the general problems encountered enticing enough to continue.  


## Coding philosophy

- The code in this repo is meant to be experimental. I try to adhere to good standards for obvious reasons but in order not to get bogged down in perfection, lenience is applied where feasible. Code that ends up generic enough to be reuse in other embedded projects are migrated to  [Spine](https://github.com/s-t-a-n/Spine), the embedded generics library that powers this project.

## Design philosophy

- keep all configurable parameters inside the main.cpp so any arbitrary settings end up in a single place
- keep the various components as modular as possible


## Schematic overview

![](https://github.com/s-t-a-n/kaskas/blob/master/doc/kaskas_schema.png?raw=true) 

## How to run

1. Get PlatformIO.
2. Run `pio run` in the root of repository to compile

## How to test

1. Get PlatformIO.
2. Run `pio test -e unittest` in the root of repository to compile

## Contribute

Feel free to open an issue if you like the project, have an idea or would like to collaborate on something else!

## References

- [Hexagonal architecture](https://alistair.cockburn.us/hexagonal-architecture/)
- [Modern C++ White paper:
  Making things do stuff](https://www.feabhas.com/sites/default/files/uploads/EmbeddedWisdom/Feabhas%20Modern%20C%2B%2B%20white%20paper%20Making%20things%20do%20stuff.pdf)
- [Monitor plant growth with AI and OpenCV
  ](https://magpi.raspberrypi.com/articles/monitor-plant-growth-ai-opencv)