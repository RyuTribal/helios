using System.Reflection;
using System.Runtime.Loader;

namespace Helios.Bridge;

internal class ScriptAssemblyLoadContext : AssemblyLoadContext
{
    public ScriptAssemblyLoadContext() : base(isCollectible: true) { }

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        // User scripts reference ScriptCore — resolve it from the default (non-collectible) context
        // where ScriptCore is already loaded.
        if (assemblyName.Name == "ScriptCore")
            return typeof(Entity).Assembly;

        return null;
    }
}
