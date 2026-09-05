.segment "FONT"
.incbin "charset.bin"
.segment "SPRITES"
.incbin "sprites.bin"
.segment "WORLD"
.export _world
_world: .incbin "world.bin"
