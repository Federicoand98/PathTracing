# Live-link Blender → Path Tracer

Sync a senso unico: **Blender esporta, il renderer legge**. Vedi [ADR 0003](../../docs/adr/0003-interop-blender-via-gltf.md).

## Setup (una volta)

1. Blender → *Edit > Preferences > Add-ons > Install…* → scegli [`pt_livelink.py`](pt_livelink.py) → abilita **Path Tracer Live-Link**.
2. **Salva il `.blend`** almeno una volta: l'export ha bisogno di sapere dove sta il file.
3. Nella *Sidebar* del viewport (tasto `N`) → tab **Path Tracer** → premi **Esporta glTF ora** (o salva di nuovo).

L'export finisce in `<cartella-del-blend>/pt_live/scene.gltf`.

## Uso quotidiano

1. Nel renderer: incolla il path di `pt_live/scene.gltf` nel campo **glTF path** → **Load glTF**. Lascia **Live-link** attivo.
2. In Blender: lavora e **salva** (`Ctrl+S`). L'addon riesporta, il renderer se ne accorge (osserva l'mtime del file) e ricarica da solo.

## Cosa passa e cosa no

**Passa:** mesh (triangoli, normali, UV), transform dei nodi, materiali Principled BSDF nel sottoinsieme mappabile (base color + texture, metallic, roughness, IOR, transmission, emissione).

**Non passa (ancora):** luci, camera, node graph procedurali, subsurface/coat/sheen/anisotropy. Senza luci importate la scena è illuminata da un cielo di default. È il "dove c'è matching" previsto dall'ADR 0003.

## Note

- L'export è **GLTF_SEPARATE** (`.gltf` + `.bin` + PNG): il renderer carica le texture da file, non da un `.glb` con immagini embedded.
- Se salvi mentre il renderer sta leggendo un file a metà scrittura, il parse fallisce e la scena resta com'è: al salvataggio successivo si riallinea.
