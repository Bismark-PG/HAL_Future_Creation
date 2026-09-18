using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;

// Editor-only migration utility. No vendor MonoBehaviours are needed or executed.
public static class StadiumExporter
{
    [Serializable] public class MeshRecord { public string id, name, source; public int vertices, triangles, submeshes; public float[] bounds; }
    [Serializable] public class MaterialRecord { public string id, name, source, texture; public float[] color, uvScale, uvOffset; public float metallic, smoothness; }
    [Serializable] public class NodeRecord { public string name, path, mesh; public bool active, variantVisible; public float[] matrix; public string[] materials; }
    [Serializable] public class ExportRecord { public string scene; public List<MeshRecord> meshes = new List<MeshRecord>(); public List<MaterialRecord> materials = new List<MaterialRecord>(); public List<NodeRecord> nodes = new List<NodeRecord>(); public List<string> skipped = new List<string>(); }
    static string output;
    static ExportRecord result;
    static Dictionary<Mesh, string> meshes = new Dictionary<Mesh, string>();
    static Dictionary<Material, string> materials = new Dictionary<Material, string>();
    static string F(float value) => value.ToString("R", CultureInfo.InvariantCulture);
    static string PathOf(Transform t) => t.parent ? PathOf(t.parent) + "/" + t.name : t.name;

    public static void Export()
    {
        try
        {
            output = Path.GetFullPath(Path.Combine(Application.dataPath, "../../Export"));
            Directory.CreateDirectory(output + "/Meshes");
            Directory.CreateDirectory(output + "/Textures");
            string scene = "Assets/ithappy/Battle_Stadium/Scenes/Built-In_Scenes/Demonstration.unity";
            var loaded = EditorSceneManager.OpenScene(scene, OpenSceneMode.Single);
            result = new ExportRecord { scene = scene };
            foreach (var root in loaded.GetRootGameObjects())
            foreach (var renderer in root.GetComponentsInChildren<Renderer>(true))
            {
                Mesh mesh = null;
                var filter = renderer.GetComponent<MeshFilter>();
                if (renderer is MeshRenderer && filter) mesh = filter.sharedMesh;
                else if (renderer is SkinnedMeshRenderer skinned)
                {
                    mesh = new Mesh { name = skinned.sharedMesh ? skinned.sharedMesh.name + "_StaticPose" : "StaticPose" };
                    skinned.BakeMesh(mesh, true);
                }
                if (!mesh) { result.skipped.Add(renderer.GetType().Name + ":" + PathOf(renderer.transform)); continue; }
                // BakeMesh vertices are relative to the SkinnedMeshRenderer's Transform,
                // not its internal root-bone rendering matrix.
                var m = renderer.transform.localToWorldMatrix;
                var matrix = new float[16];
                for (int row = 0; row < 4; row++) for (int col = 0; col < 4; col++) matrix[row * 4 + col] = m[row, col];
                bool variantVisible = renderer.enabled;
                for (Transform parent = renderer.transform; parent; parent = parent.parent)
                    if (parent.name != "Levels" && !(parent.parent && parent.parent.name == "Levels")) variantVisible &= parent.gameObject.activeSelf;
                result.nodes.Add(new NodeRecord {
                    name = renderer.name, path = PathOf(renderer.transform), mesh = ExportMesh(mesh),
                    active = renderer.enabled && renderer.gameObject.activeInHierarchy, variantVisible = variantVisible, matrix = matrix,
                    materials = renderer.sharedMaterials.Select(ExportMaterial).ToArray()
                });
            }
            File.WriteAllText(output + "/scene.json", JsonUtility.ToJson(result, true));
            Debug.Log("STADIUM_EXPORT_OK nodes=" + result.nodes.Count + " meshes=" + result.meshes.Count + " materials=" + result.materials.Count);
            EditorApplication.Exit(0);
        }
        catch (Exception e) { Debug.LogException(e); EditorApplication.Exit(1); }
    }

    static string ExportMesh(Mesh mesh)
    {
        if (meshes.TryGetValue(mesh, out var existing)) return existing;
        string id = "SM_" + meshes.Count.ToString("D4") + "_" + System.Text.RegularExpressions.Regex.Replace(mesh.name, "[^a-zA-Z0-9_]", "_");
        meshes.Add(mesh, id);
        var vertices = mesh.vertices; var normals = mesh.normals; var uv = mesh.uv;
        using (var writer = new StreamWriter(output + "/Meshes/" + id + ".obj"))
        {
            writer.WriteLine("# Unity local geometry, RH Z-up centimeters for Unreal legacy FBX/OBJ importer");
            writer.WriteLine("mtllib " + id + ".mtl");
            writer.WriteLine("o " + id);
            foreach (var v in vertices) writer.WriteLine("v " + F(v.z * 100) + " " + F(-v.x * 100) + " " + F(v.y * 100));
            for (int i = 0; i < vertices.Length; i++) { var v = uv.Length == vertices.Length ? uv[i] : Vector2.zero; writer.WriteLine("vt " + F(v.x) + " " + F(v.y)); }
            for (int i = 0; i < vertices.Length; i++) { var n = normals.Length == vertices.Length ? normals[i] : Vector3.up; writer.WriteLine("vn " + F(n.z) + " " + F(-n.x) + " " + F(n.y)); }
            for (int sub = 0; sub < mesh.subMeshCount; sub++)
            {
                writer.WriteLine("usemtl Slot_" + sub.ToString("D2"));
                var triangles = mesh.GetTriangles(sub);
                for (int i = 0; i < triangles.Length; i += 3)
                {
                    int a = triangles[i] + 1, b = triangles[i + 2] + 1, c = triangles[i + 1] + 1;
                    writer.WriteLine($"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}");
                }
            }
        }
        using (var writer = new StreamWriter(output + "/Meshes/" + id + ".mtl"))
            for (int sub = 0; sub < mesh.subMeshCount; sub++) writer.WriteLine("newmtl Slot_" + sub.ToString("D2") + "\nKd 1 1 1\n");
        var bounds = mesh.bounds;
        result.meshes.Add(new MeshRecord { id = id, name = mesh.name, source = AssetDatabase.GetAssetPath(mesh), vertices = vertices.Length, triangles = mesh.triangles.Length / 3, submeshes = mesh.subMeshCount,
            bounds = new [] { bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.max.y, bounds.max.z } });
        return id;
    }

    static string ExportMaterial(Material material)
    {
        if (!material) return "";
        if (materials.TryGetValue(material, out var existing)) return existing;
        string id = "M_" + materials.Count.ToString("D3") + "_" + System.Text.RegularExpressions.Regex.Replace(material.name, "[^a-zA-Z0-9_]", "_");
        materials.Add(material, id);
        Color color = Color.white; Texture texture = null; Vector2 uvScale = Vector2.one, uvOffset = Vector2.zero;
        var serialized = new SerializedObject(material);
        var colors = serialized.FindProperty("m_SavedProperties.m_Colors");
        if (colors != null) for (int i = 0; i < colors.arraySize; i++) {
            var e = colors.GetArrayElementAtIndex(i); string key = e.FindPropertyRelative("first").stringValue;
            if (key == "_Color" || key == "_BaseColor") color = e.FindPropertyRelative("second").colorValue;
        }
        var textures = serialized.FindProperty("m_SavedProperties.m_TexEnvs");
        if (textures != null) for (int i = 0; i < textures.arraySize; i++) {
            var e = textures.GetArrayElementAtIndex(i); string key = e.FindPropertyRelative("first").stringValue;
            if (key == "_MainTex" || key == "_BaseMap" || key == "_BaseColorMap") {
                var candidate = e.FindPropertyRelative("second.m_Texture").objectReferenceValue as Texture;
                if (candidate) { texture = candidate; uvScale = e.FindPropertyRelative("second.m_Scale").vector2Value; uvOffset = e.FindPropertyRelative("second.m_Offset").vector2Value; }
            }
        }
        string texturePath = texture ? AssetDatabase.GetAssetPath(texture) : "";
        string destination = "";
        if (!string.IsNullOrEmpty(texturePath) && File.Exists(texturePath)) {
            destination = AssetDatabase.AssetPathToGUID(texturePath) + Path.GetExtension(texturePath);
            File.Copy(texturePath, output + "/Textures/" + destination, true);
        }
        float metallic = 0, smoothness = 0.3f;
        var floats = serialized.FindProperty("m_SavedProperties.m_Floats");
        if (floats != null) for (int i = 0; i < floats.arraySize; i++) {
            var e = floats.GetArrayElementAtIndex(i); string key = e.FindPropertyRelative("first").stringValue;
            if (key == "_Metallic") metallic = e.FindPropertyRelative("second").floatValue;
            if (key == "_Glossiness" || key == "_Smoothness") smoothness = e.FindPropertyRelative("second").floatValue;
        }
        result.materials.Add(new MaterialRecord { id = id, name = material.name, source = AssetDatabase.GetAssetPath(material), texture = destination, color = new [] { color.r, color.g, color.b, color.a }, uvScale = new [] { uvScale.x, uvScale.y }, uvOffset = new [] { uvOffset.x, uvOffset.y }, metallic = metallic, smoothness = smoothness });
        return id;
    }
}
