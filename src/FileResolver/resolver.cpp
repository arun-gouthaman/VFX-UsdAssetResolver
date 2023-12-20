#include "resolver.h"
#include "resolverContext.h"

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/pyInvoke.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"


#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <regex>

PXR_NAMESPACE_OPEN_SCOPE

AR_DEFINE_RESOLVER(FileResolver, ArResolver);

static bool
_IsRelativePath(const std::string& path)
{
    return (!path.empty() && TfIsRelativePath(path));
}

static bool
_IsFileRelativePath(const std::string& path) {
    return path.find("./") == 0 || path.find("../") == 0;
}

static bool
_IsSearchPath(const std::string& path)
{
    return _IsRelativePath(path) && !_IsFileRelativePath(path);
}

static std::string
_AnchorRelativePath(
    const std::string& anchorPath, 
    const std::string& path)
{
    if (TfIsRelativePath(anchorPath) ||
        !_IsRelativePath(path)) {
        return path;
    }
    // Ensure we are using forward slashes and not back slashes.
    std::string forwardPath = anchorPath;
    std::replace(forwardPath.begin(), forwardPath.end(), '\\', '/');
    // If anchorPath does not end with a '/', we assume it is specifying
    // a file, strip off the last component, and anchor the path to that
    // directory.
    const std::string anchoredPath = TfStringCatPaths(TfStringGetBeforeSuffix(forwardPath, '/'), path);
    return TfNormPath(anchoredPath);
}

static ArResolvedPath
_ResolveAnchored(
    const std::string& anchorPath,
    const std::string& path)
{
    std::string resolvedPath = path;
    if (!anchorPath.empty()) {
        resolvedPath = TfStringCatPaths(anchorPath, path);
    }
    return TfPathExists(resolvedPath) ? ArResolvedPath(TfAbsPath(resolvedPath)) : ArResolvedPath();
}

FileResolver::FileResolver() = default;

FileResolver::~FileResolver() = default;

std::string
FileResolver::_CreateIdentifier(
    const std::string& assetPath,
    const ArResolvedPath& anchorAssetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("Resolver::_CreateIdentifier('%s', '%s')\n",
                                        assetPath.c_str(), anchorAssetPath.GetPathString().c_str());

    // EDIT returning path passed it with no modifications
    return assetPath;

    if (assetPath.empty()) {
        return assetPath;
    }

    if (!anchorAssetPath) {
        return TfNormPath(assetPath);
    }

    const std::string anchoredAssetPath = _AnchorRelativePath(anchorAssetPath, assetPath);

    if (_IsSearchPath(assetPath) && Resolve(anchoredAssetPath).empty()) {
        return TfNormPath(assetPath);
    }

    return TfNormPath(anchoredAssetPath);
}

std::string
FileResolver::_CreateIdentifierForNewAsset(
    const std::string& assetPath,
    const ArResolvedPath& anchorAssetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::_CreateIdentifierForNewAsset"
        "('%s', '%s')\n",
        assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    if (assetPath.empty()) {
        return assetPath;
    }

    if (_IsRelativePath(assetPath)) {
        return TfNormPath(anchorAssetPath ? 
            _AnchorRelativePath(anchorAssetPath, assetPath) :
            TfAbsPath(assetPath));
    }

    return TfNormPath(assetPath);
}



ArResolvedPath
FileResolver::_Resolve(
    const std::string& assetPath) const
{
    const FileResolverContext* contexts[2] = { this->_GetCurrentContextPtr(), &_fallbackContext };

    TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve !!!!!\n\n");

    for (const FileResolverContext* ctx : contexts)
    {
        if (!ctx)
        {
            continue;
        }
        std::map<std::string, std::string> mapping_pairs = ctx->GetMappingPairs();

        std::map<std::string, std::string>::iterator mapped_pair_it = mapping_pairs.find(assetPath);


        TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve Checking for !!!!!\n('%s')\n\n", assetPath.c_str());
        if (mapped_pair_it != mapping_pairs.end())
        {
            //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve get mapped pair !!!!!\n'(%s)' - '(%s)'\n\n", assetPath.c_str(), mapped_pair_it->second.c_str());
            TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), mapped_pair_it->second.c_str());
            return _ResolveAnchored(std::string(), mapped_pair_it->second);
        }
        else
        {
            TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve DEFAULT IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), assetPath.c_str());
            pxr::ArResolvedPath res_path = _ResolveAnchored(std::string(), assetPath);
            for (std::string search_path : ctx->GetSearchPaths())
            {

                if (search_path.empty())
                {
                    continue;
                }
                ArResolvedPath resolvedPath = _ResolveAnchored(search_path, assetPath);
                if (!resolvedPath.empty())
                {
                    return resolvedPath;
                }
            }
            //return _ResolveAnchored(std::string(), assetPath);
            //return ArResolvedPath();
        }
    }
    //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve returning default !!!!! '(%s)'\n\n", assetPath.c_str());
    //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), assetPath.c_str());
    //return _ResolveAnchored(std::string(), assetPath);
    return _ResolveAnchored(std::string(), assetPath);
}

/*
ArResolvedPath
FileResolver::_Resolve(
    const std::string& assetPath) const
{
    TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _RESOLVE TEST !!!!! ('%s')\n", assetPath.c_str());
    if (assetPath.empty()) {
        return ArResolvedPath();
    }
    if (_IsRelativePath(assetPath)) {
        TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("RELATIVE PATH\n");
        if (this->_IsContextDependentPath(assetPath)) {
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("CONTEXT DEPENDENT\n");
            const FileResolverContext* contexts[2] = {this->_GetCurrentContextPtr(), &_fallbackContext};
            for (const FileResolverContext* ctx : contexts) {
                if (ctx) {
                    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("!!!!! CONTEXT FOUND !!!!!\n");
                    auto &mappingPairs = ctx->GetMappingPairs();
                    std::string mappedPath = assetPath;
                    if (!mappingPairs.empty()){
                        if (!ctx->GetMappingRegexExpressionStr().empty())
                        {
                            mappedPath = std::regex_replace(mappedPath,
                                                            ctx->GetMappingRegexExpression(),
                                                            ctx->GetMappingRegexFormat());
                            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s')"
                                                                        " - Mapped to '%s' via regex expression '%s' with formatting '%s'\n", 
                                                                        assetPath.c_str(),
                                                                        mappedPath.c_str(),
                                                                        ctx->GetMappingRegexExpressionStr().c_str(),
                                                                        ctx->GetMappingRegexFormat().c_str());
                        }
                    }
                    auto map_find = mappingPairs.find(mappedPath);
                    if(map_find != mappingPairs.end()){
                        mappedPath = map_find->second;
                    }
                    for (const auto& searchPath : ctx->GetSearchPaths()) {
                        ArResolvedPath resolvedPath = _ResolveAnchored(searchPath, mappedPath);
                        if (resolvedPath) {
                            return resolvedPath;
                        }
                    }
                    // Only try the first valid context.
                    break;
                }
                else
                {
                    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("!!!!! CONTEXT NOT FOUND !!!!!\n");
                }
            }
        }
        return ArResolvedPath();
    }
    else
    {
        const FileResolverContext* contexts[2] = { this->_GetCurrentContextPtr(), &_fallbackContext };
        for (const FileResolverContext* ctx : contexts)
        {
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("ELSE PATH\n");
            if (ctx) 
            {
                TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("CTX FOUND\n");
                std::map<std::string, std::string> map_pair = ctx->GetMappingPairs();
                if (!map_pair.empty())
                {
                    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("PAIR FOUND\n");
                    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("CHECK KEY ('%s')\n", assetPath.c_str());
                    std::map<std::string, std::string>::iterator map_find = map_pair.find(assetPath);
                    if (map_find != map_pair.end())
                    {
                        TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("KEY FOUND\n");
                        TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("PAIR ('%s') -> ('%s')\n", assetPath.c_str(), map_find->second);
                        return _ResolveAnchored(std::string(), map_find->second);
                    }
                    else
                    {
                        TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("KEY NOT FOUND\n");
                    }
                }
            }
        }
    }
    return _ResolveAnchored(std::string(), assetPath);
}

*/

ArResolvedPath
FileResolver::_ResolveForNewAsset(
    const std::string& assetPath) const
{
    TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset !!!!!\n\n");
    const FileResolverContext* contexts[2] = { this->_GetCurrentContextPtr(), &_fallbackContext };

    for (const FileResolverContext* ctx : contexts)
    {
        if (!ctx)
        {
            continue;
        }
        std::map<std::string, std::string> mapping_pairs = ctx->GetMappingPairs();

        std::map<std::string, std::string>::iterator mapped_pair_it = mapping_pairs.find(assetPath);



        TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset Checking for !!!!!\n('%s')\n\n", assetPath.c_str());
        if (mapped_pair_it != mapping_pairs.end())
        {
            //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve get mapped pair !!!!!\n'(%s)' - '(%s)'\n\n", assetPath.c_str(), mapped_pair_it->second.c_str());
            TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), mapped_pair_it->second.c_str());
            return _ResolveAnchored(std::string(), mapped_pair_it->second);
        }
        else
        {
            TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset DEFAULT IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), assetPath.c_str());
            pxr::ArResolvedPath res_path = _ResolveAnchored(std::string(), assetPath);
            TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset Search Path!!!!! \n");
            for (std::string search_path : ctx->GetSearchPaths())
            {
                TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("Path - > %s\n\n", search_path.c_str());
                if (search_path.empty())
                {
                    continue;
                }
                TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve for new asset Search Path !!!!! ('%s')\n", search_path.c_str());
                ArResolvedPath resolvedPath = _ResolveAnchored(search_path, assetPath);
                if (!resolvedPath.empty())
                {
                    return resolvedPath;
                }
            }
            return _ResolveAnchored(std::string(), assetPath);
        }
    }
    //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve returning default !!!!! '(%s)'\n\n", assetPath.c_str());
    //TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("!!!!! _Resolve IP-OP !!!!! ('%s') - ('%s')\n\n\n", assetPath.c_str(), assetPath.c_str());
    //return _ResolveAnchored(std::string(), assetPath);
    return ArResolvedPath();
    //TF_DEBUG(FILERESOLVER_RESOLVER).Msg("Resolver::_ResolveForNewAsset('%s')\n", assetPath.c_str());
    //return ArResolvedPath(assetPath.empty() ? assetPath : TfAbsPath(assetPath));
}

ArResolverContext
FileResolver::_CreateDefaultContext() const
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContext()\n");
    return _fallbackContext;
}

ArResolverContext
FileResolver::_CreateDefaultContextForAsset(
    const std::string& assetPath) const
{
    // As there can be only one context class instance per context class
    // we automatically skip creation of contexts if it exists (The performance heavy
    // part is the pinning data creation). Here we can return any existing instance of
    // a FileResolverContext, thats why we just use the fallback context.
    // See for more info: https://openusd.org/release/api/class_ar_resolver_context.html
    // > Note that an ArResolverContext may not hold multiple context objects with the same type.
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s')\n", assetPath.c_str());
    // Fallback to existing context
    if (assetPath.empty()){
        return ArResolverContext(_fallbackContext);
    }
    ArResolvedPath resolvedPath = this->_Resolve(assetPath);
    if (!TfPathExists(resolvedPath)){
        return ArResolverContext(_fallbackContext);
    }
    std::string resolvedPathStr = resolvedPath.GetPathString();
    if(this->_GetCurrentContextObject<FileResolverContext>() != nullptr){
        TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Skipping on same stage\n", assetPath.c_str());
        return ArResolverContext(_fallbackContext);
    }
    auto map_iter = _sharedContexts.find(resolvedPath);
    if(map_iter != _sharedContexts.end()){
        if (map_iter->second.timestamp.GetTime() == this->_GetModificationTimestamp(assetPath, resolvedPath).GetTime())
        {
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing context on different stage\n", assetPath.c_str());
            return ArResolverContext(map_iter->second.ctx);
        }else{
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing context on different stage, reloading due to changed timestamp\n", assetPath.c_str());
            map_iter->second.ctx.RefreshFromMappingFilePath();
            return ArResolverContext(map_iter->second.ctx);
        }
    }
    // Create new context
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Constructing new context\n", assetPath.c_str());
    std::string assetDir = TfGetPathName(TfAbsPath(resolvedPathStr));
    struct FileResolverContextRecord record;
    record.timestamp = this->_GetModificationTimestamp(assetPath, resolvedPath);
    record.ctx = FileResolverContext(resolvedPath, std::vector<std::string>(1, assetDir));
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Constructing new context assetDir\n", assetDir.c_str());
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Constructing new context resolvedPath\n", resolvedPath.GetPathString().c_str());
    _sharedContexts.insert(std::pair<std::string, FileResolverContextRecord>(resolvedPath, record));
    return ArResolverContext(record.ctx);
}

bool
FileResolver::_IsContextDependentPath(
    const std::string& assetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_IsContextDependentPath('%s')\n", assetPath.c_str());
    return _IsSearchPath(assetPath);
}

void
FileResolver::_RefreshContext(
    const ArResolverContext& context)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_RefreshContext()\n");
    const FileResolverContext* ctx = this->_GetCurrentContextPtr();
    if (!ctx) {
        return;
    }
    ArNotice::ResolverChanged(*ctx).Send();
}

ArTimestamp
FileResolver::_GetModificationTimestamp(
    const std::string& assetPath,
    const ArResolvedPath& resolvedPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::GetModificationTimestamp('%s', '%s')\n",
        assetPath.c_str(), resolvedPath.GetPathString().c_str());
    return ArFilesystemAsset::GetModificationTimestamp(resolvedPath);
}

std::shared_ptr<ArAsset>
FileResolver::_OpenAsset(
    const ArResolvedPath& resolvedPath) const
{
    TF_DEBUG(FILERESOLVER_TEST_DEBUG).Msg("\n!!!! OPEN ASSET !!!! ('%s')\n\n", resolvedPath.GetPathString().c_str());
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::_OpenAsset('%s')\n",
        resolvedPath.GetPathString().c_str());
    return ArFilesystemAsset::Open(resolvedPath);
}

std::shared_ptr<ArWritableAsset>
FileResolver::_OpenAssetForWrite(
    const ArResolvedPath& resolvedPath,
    WriteMode writeMode) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::_OpenAssetForWrite('%s', %d)\n",
        resolvedPath.GetPathString().c_str(),
        static_cast<int>(writeMode));
    return ArFilesystemWritableAsset::Create(resolvedPath, writeMode);
}

const FileResolverContext* 
FileResolver::_GetCurrentContextPtr() const
{
    return _GetCurrentContextObject<FileResolverContext>();
}

PXR_NAMESPACE_CLOSE_SCOPE