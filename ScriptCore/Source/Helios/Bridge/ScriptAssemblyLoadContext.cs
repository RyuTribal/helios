using System.Reflection;
using System.Runtime.Loader;

namespace Helios.Bridge;

internal class ScriptAssemblyLoadContext : AssemblyLoadContext
{
    public ScriptAssemblyLoadContext() : base(isCollectible: true) { }
    protected override Assembly? Load(AssemblyName assemblyName) => null;
}
