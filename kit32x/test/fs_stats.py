"""Résume fs.log (BlastEm instrumenté) : images/s et ligne de fin du dessin.

Chaque « FS demande v=N » est l'écriture de FS par le Master : le dessin
de l'image est fini à la ligne N. Un échange par VBlank = 60 images/s.
"""
import sys

vblanks = swaps = 0
requests = []
for line in open(sys.argv[1]):
    if line.startswith("VBLK debut"):
        vblanks += 1
    elif line.startswith("FS swap"):
        swaps += 1
    elif line.startswith("FS demande"):
        requests.append(int(line.split("v=")[1]))

print(f"VBlanks 32X : {vblanks}   échanges de frame buffer : {swaps}")
if vblanks:
    print(f"images/s (60 Hz) : {60 * swaps / vblanks:.1f}")
if requests:
    tail = requests[len(requests) // 2:]   # ignorer le démarrage
    print(f"ligne de la demande d'échange (seconde moitié) : "
          f"min {min(tail)}, max {max(tail)}, moyenne {sum(tail) / len(tail):.0f}")
    print("  (une demande à la ligne 0 = le Master attendait la fin du VBlank)")
