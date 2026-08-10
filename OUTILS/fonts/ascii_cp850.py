#!/usr/bin/env python3

# Glyphes graphiques DOS (CP437) pour les codes de contrôle 0x00-0x1F et 0x7F
cp437_glyphs = {
    0: '∅',
    1: '☺', 2: '☻', 3: '♥', 4: '♦', 5: '♣', 6: '♠',
    7: '•', 8: '◘', 9: '○', 10: '◙', 11: '♂', 12: '♀',
    13: '♪', 14: '♫', 15: '☼', 16: '►', 17: '◄', 18: '↕',
    19: '‼', 20: '¶', 21: '§', 22: '▬', 23: '↨', 24: '↑',
    25: '↓', 26: '→', 27: '←', 28: '∟', 29: '↔', 30: '▲',
    31: '▼', 127: '⌂'
}

lignes = ['Dec  Hex  Char', '-' * 20]

for i in range(256):
    if i in cp437_glyphs:
        char = cp437_glyphs[i]
    else:
        char = bytes([i]).decode('cp850')
    lignes.append(f'{i:3d}  0x{i:02X}  {char}')

with open('ascii_cp850.txt', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lignes) + '\n')

print("Fichier 'ascii_cp850.txt' généré avec succès.")
