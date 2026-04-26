# Sky Miner

Original C64 game for the C64AIToolChain. The active gameplay layer uses C64 hardware sprites for the ship and falling objects.

## Objective
Move left/right to collect crystals and avoid meteors.

- Yellow crystal sprite: score points and build combo
- Red meteor sprite: lose 1 life and reset combo
- Green repair sprite: score points and restore 1 life, up to 5
- Every 120 points: wave increases and falling speed improves
- Demo mode auto-starts after a short wait on the title screen

## Controls
- Joystick Port 2: left/right to move
- Fire: start/restart manual play
- Wait on the title screen: demo autoplay

## Build
```bash
./build.sh
```

## Run
```bash
./run_vice.sh
```
