依赖关系：
    应用层
    ↓
    PluginManager.h (接口)
    ↓
    PluginManager.cpp (实现)
    ↓
    PluginManagerImpl.h (内部类)
    ↓
    DynamicLibraryLoader.h (接口)
    ↓
    DynamicLibraryLoader.cpp (实现)
    ↓
    DynamicLibraryLoaderImpl.h (内部类)
    ↓
    操作系统API (dlopen/LoadLibrary)