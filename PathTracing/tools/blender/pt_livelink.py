# Addon Blender per il live-link col path tracer (ADR 0003).
#
# A ogni salvataggio del .blend esporta la scena in glTF non-binary (.gltf + .bin + texture PNG)
# in una cartella accanto al file. Il renderer osserva quel .gltf (mtime) e ricarica da solo.
# Il flusso e' a senso unico: Blender esporta, il renderer legge. Blender non viene mai riscritto.
#
# INSTALLAZIONE:
#   Blender > Edit > Preferences > Add-ons > Install... > scegli questo file > abilita
#   "Path Tracer Live-Link". Poi salva il .blend: la prima esportazione parte da li'.
#   Il glTF finisce in  <cartella-del-blend>/pt_live/scene.gltf  -> e' il path da aprire nel renderer.

bl_info = {
    "name": "Path Tracer Live-Link",
    "author": "Federico Andrucci",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > Path Tracer",
    "description": "Esporta la scena in glTF a ogni salvataggio, per il live-link col path tracer",
    "category": "Import-Export",
}

import os
import bpy
from bpy.app.handlers import persistent


def _export_dir():
    """Cartella di export accanto al .blend. None se il file non e' ancora stato salvato."""
    blend = bpy.data.filepath
    if not blend:
        return None
    return os.path.join(os.path.dirname(blend), "pt_live")


def export_gltf():
    """Esporta la scena corrente in <blenddir>/pt_live/scene.gltf. Ritorna il path o None."""
    out_dir = _export_dir()
    if out_dir is None:
        print("[PT Live-Link] .blend non salvato: niente da esportare")
        return None

    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "scene.gltf")

    # GLTF_SEPARATE: .gltf + .bin + PNG separati. Il renderer carica le texture da FILE, non
    # da un GLB con immagini embedded (vedi ADR 0003). +Y up e' il default glTF, coerente col
    # sistema di coordinate del renderer.
    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLTF_SEPARATE',
        export_apply=True,        # applica i modifier (il renderer vede triangoli finali)
        export_yup=True,
        export_normals=True,
        export_texcoords=True,
        export_materials='EXPORT',
        use_selection=False,
    )
    print("[PT Live-Link] esportato:", out_path)
    return out_path


@persistent
def _on_save(dummy):
    # save_post gira DOPO il salvataggio del .blend. L'export non e' un salvataggio, quindi
    # non ri-innesca questo handler: nessuna ricorsione.
    if bpy.context.scene.pt_livelink_enabled:
        export_gltf()


class PT_OT_export_now(bpy.types.Operator):
    """Esporta subito la scena in glTF per il renderer"""
    bl_idname = "pt.export_now"
    bl_label = "Esporta glTF ora"

    def execute(self, context):
        path = export_gltf()
        if path is None:
            self.report({'WARNING'}, "Salva prima il .blend")
            return {'CANCELLED'}
        self.report({'INFO'}, "Esportato: " + path)
        return {'FINISHED'}


class PT_PT_panel(bpy.types.Panel):
    bl_label = "Path Tracer Live-Link"
    bl_idname = "PT_PT_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Path Tracer"

    def draw(self, context):
        col = self.layout.column()
        col.prop(context.scene, "pt_livelink_enabled", text="Esporta al salvataggio")
        col.operator("pt.export_now", icon='EXPORT')
        d = _export_dir()
        col.label(text=("Output: " + d) if d else "Salva prima il .blend")


def register():
    bpy.types.Scene.pt_livelink_enabled = bpy.props.BoolProperty(
        name="Live-link al salvataggio", default=True)
    bpy.utils.register_class(PT_OT_export_now)
    bpy.utils.register_class(PT_PT_panel)
    if _on_save not in bpy.app.handlers.save_post:
        bpy.app.handlers.save_post.append(_on_save)


def unregister():
    if _on_save in bpy.app.handlers.save_post:
        bpy.app.handlers.save_post.remove(_on_save)
    bpy.utils.unregister_class(PT_PT_panel)
    bpy.utils.unregister_class(PT_OT_export_now)
    del bpy.types.Scene.pt_livelink_enabled


if __name__ == "__main__":
    register()
