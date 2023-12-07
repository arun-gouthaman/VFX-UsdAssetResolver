#define CONVERT_STRING(string) #string
#define DEFINE_STRING(string) CONVERT_STRING(string)

#include "resolverContext.h"
#include "resolverTokens.h"

#include "pxr/pxr.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/pathUtils.h"
#include <pxr/usd/sdf/layer.h>
#include <pxr/base/js/json.h>

#include <iostream>
#include <vector>

#include <fstream>

PXR_NAMESPACE_USING_DIRECTIVE

bool getStringEndswithString(const std::string &value, const std::string &compareValue)
{
    if (compareValue.size() > value.size())
    {
        return false;
    }
    if (std::equal(compareValue.rbegin(), compareValue.rend(), value.rbegin()))
    {
        return true;
    }
    return false;
}

bool getStringEndswithStrings(const std::string &value, const std::vector<std::string> array)
{
    for (int i=0; i < array.size(); i++)
    {
        if (getStringEndswithString(value, array[i]))
        {
            return true;
        }
    }
    return false;
}

FileResolverContext::FileResolverContext() {
    // Init
    this->_LoadEnvMappingRegex();
    this->RefreshSearchPaths();
}

FileResolverContext::FileResolverContext(const FileResolverContext& ctx) = default;

FileResolverContext::FileResolverContext(const std::string& mappingFilePath)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST 1\n");
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("ResolverContext::ResolverContext('%s') - Creating new context\n", mappingFilePath.c_str());
    // Init
    this->_LoadEnvMappingRegex();
    this->RefreshSearchPaths();
    this->SetMappingFilePath(TfAbsPath(mappingFilePath));
    this->_GetMappingPairsFromUsdFile(this->GetMappingFilePath());
}

FileResolverContext::FileResolverContext(const std::vector<std::string>& searchPaths)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST 2\n");
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("ResolverContext::ResolverContext() - Creating new context with custom search paths\n");
    // Init
    this->_GetMappingPairsFromJsonFile(searchPaths[0]);
    this->_LoadEnvMappingRegex();
    this->SetCustomSearchPaths(searchPaths);
    this->RefreshSearchPaths();
}

FileResolverContext::FileResolverContext(const std::string& mappingFilePath, const std::vector<std::string>& searchPaths)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST 3\n");
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("ResolverContext::ResolverContext('%s') - Creating new context with custom search paths\n", mappingFilePath.c_str());
    // Init
    this->_LoadEnvMappingRegex();
    this->SetCustomSearchPaths(searchPaths);
    this->RefreshSearchPaths();
    this->SetMappingFilePath(TfAbsPath(mappingFilePath));
    this->_GetMappingPairsFromUsdFile(this->GetMappingFilePath());
    this->_GetMappingPairsFromJsonFile(this->GetMappingFilePath());
    this->GetMappingPair();
}

bool
FileResolverContext::operator<(
    const FileResolverContext& ctx) const
{
    // This is a no-op for now, as it doesn't get used for now.
    return true;
}

bool
FileResolverContext::operator==(
    const FileResolverContext& ctx) const
{
    return this->GetMappingFilePath() == ctx.GetMappingFilePath();
}

bool
FileResolverContext::operator!=(
    const FileResolverContext& ctx) const
{
    return this->GetMappingFilePath() != ctx.GetMappingFilePath();
}

size_t hash_value(const FileResolverContext& ctx)
{
    return TfHash()(ctx.GetMappingFilePath());
}

void
FileResolverContext::_LoadEnvMappingRegex()
{
    data->mappingRegexExpressionStr = TfGetenv(DEFINE_STRING(AR_ENV_SEARCH_REGEX_EXPRESSION));
    data->mappingRegexExpression = std::regex(data->mappingRegexExpressionStr);
    data->mappingRegexFormat = TfGetenv(DEFINE_STRING(AR_ENV_SEARCH_REGEX_FORMAT));
}

void
FileResolverContext::_LoadEnvSearchPaths()
{
    data->envSearchPaths.clear();
    const std::string envSearchPathsStr = TfGetenv(DEFINE_STRING(AR_ENV_SEARCH_PATHS));
    if (!envSearchPathsStr.empty()) {
        const std::vector<std::string> envSearchPaths = TfStringTokenize(envSearchPathsStr, ARCH_PATH_LIST_SEP);
        for (const std::string& envSearchPath : envSearchPaths) {
            if (envSearchPath.empty()) { continue; }
            const std::string absEnvSearchPath = TfAbsPath(envSearchPath);
            if (absEnvSearchPath.empty()) {
                TF_WARN(
                    "Could not determine absolute path for search path prefix "
                    "'%s'", envSearchPath.c_str());
                continue;
            }
            data->envSearchPaths.push_back(absEnvSearchPath);
        }
    }
}

bool FileResolverContext::_GetMappingPairsFromUsdFile(const std::string& filePath)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("ResolverContext::ResolverContext('%s') - Checking for USD mapping pairs\n", filePath.c_str());
    data->mappingPairs.clear();
    std::vector<std::string> usdFilePathExts{ ".usd", ".usdc", ".usda" };
    if (!getStringEndswithStrings(filePath, usdFilePathExts))
    {
        return false;
    }
    auto layer = SdfLayer::FindOrOpen(TfAbsPath(filePath));
    if (!layer){
        return false;
    }
    auto layerMetaData = layer->GetCustomLayerData();
    auto mappingDataPtr = layerMetaData.GetValueAtPath(FileResolverTokens->mappingPairs);
    if (!mappingDataPtr){
        return false;
    }
    pxr::VtStringArray mappingDataArray = mappingDataPtr->Get<pxr::VtStringArray>();
    if (mappingDataArray.size() % 2 != 0){
        return false;
    }
    for (size_t i = 0; i < mappingDataArray.size(); i+=2) {
        this->AddMappingPair(mappingDataArray[i], mappingDataArray[i+1]);
    }
    return true;
}

bool FileResolverContext::_GetMappingPairsFromJsonFile(const std::string& filePath)
{
    std::string assetDir = TfGetPathName(TfAbsPath(filePath));
    std::string replaceJsonFilePath = TfNormPath(
        TfStringCatPaths(assetDir, "replace_json.json"));
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("ResolverContext::_GetMappingPairsFromJsonFile ('%s')\n", filePath.c_str());
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("ResolverContext::_GetMappingPairsFromJsonFile ('%s')\n", assetDir.c_str());
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("ResolverContext::_GetMappingPairsFromJsonFile ('%s')\n", replaceJsonFilePath.c_str());

    std::ifstream ifs(replaceJsonFilePath);


    if (ifs)
    {
        TF_DEBUG(FILERESOLVER_RESOLVER).Msg("ResolverContext::_GetMappingPairsFromJsonFile JSON FILE FOUND\n");
        JsParseError error;
        const JsValue value = JsParseStream(ifs, &error);
        ifs.close();

        if (!value.IsNull() && value.IsArray()) {
            if (value.GetJsArray().size() > 0) {
                data->mappingPairs.clear();
                for (const auto& pair : value.GetJsArray()) {
                    if (pair.IsArray()) {
                        this->AddMappingPair(
                            pair.GetJsArray()[0].GetString(), pair.GetJsArray()[1].GetString());
                        TF_DEBUG(FILERESOLVER_RESOLVER).Msg("%s\n", pair.GetJsArray()[0].GetString().c_str());
                        TF_DEBUG(FILERESOLVER_RESOLVER).Msg("%s\n", pair.GetJsArray()[1].GetString().c_str());
                    }
                }
            }
        }
        else {
            fprintf(stderr, "Error: parse error at %s:%d:%d: %s\n",
                replaceJsonFilePath.c_str(), error.line, error.column, error.reason.c_str());
            return false;
        }
    }

    return true;
}

void FileResolverContext::AddMappingPair(const std::string& sourceStr, const std::string& targetStr){
    auto map_find = data->mappingPairs.find(sourceStr);
    if(map_find != data->mappingPairs.end()){
        map_find->second = targetStr;
    }else{
        data->mappingPairs.insert(std::pair<std::string, std::string>(sourceStr,targetStr));
    }
}

void FileResolverContext::GetMappingPair()
{
    for (std::pair<std::string, std::string> _pair : data->mappingPairs)
    {
        TF_DEBUG(FILERESOLVER_RESOLVER).Msg("\nResolverContext::_GetMappingPairsFromData ('%s')\n", _pair.first.c_str());
        TF_DEBUG(FILERESOLVER_RESOLVER).Msg("ResolverContext::_GetMappingPairsFromData ('%s')\n\n", _pair.second.c_str());
    }
}

void FileResolverContext::RemoveMappingByKey(const std::string& sourceStr){
    const auto &it = data->mappingPairs.find(sourceStr);
    if (it != data->mappingPairs.end()){
        data->mappingPairs.erase(it);
    }
}

void FileResolverContext::RemoveMappingByValue(const std::string& targetStr){
    for (auto it = data->mappingPairs.cbegin(); it != data->mappingPairs.cend();)
    {
        if (it->second == targetStr)
        {
            data->mappingPairs.erase(it++);
        }
        else
        {
            ++it;
        }
    }
}

void FileResolverContext::RefreshSearchPaths(){
    data->searchPaths.clear();
    this->_LoadEnvSearchPaths();
    if (!data->envSearchPaths.empty()) {
        data->searchPaths.insert(data->searchPaths.end(), data->envSearchPaths.begin(), data->envSearchPaths.end());
    }
    if (!data->customSearchPaths.empty()) {
        data->searchPaths.insert(data->searchPaths.end(), data->customSearchPaths.begin(), data->customSearchPaths.end());
    }
}

void FileResolverContext::SetCustomSearchPaths(const std::vector<std::string>& searchPaths){
    data->customSearchPaths.clear();
    if (!searchPaths.empty()) {
        for (const std::string& searchPath : searchPaths) {
            if (searchPath.empty()) { continue; }
            const std::string absSearchPath = TfAbsPath(searchPath);
            if (absSearchPath.empty()) {
                TF_WARN(
                    "Could not determine absolute path for search path prefix "
                    "'%s'", searchPath.c_str());
                continue;
            }
            data->customSearchPaths.push_back(absSearchPath);
        }
    }
}

void FileResolverContext::RefreshFromMappingFilePath(){
    this->_GetMappingPairsFromUsdFile(this->GetMappingFilePath());
}

