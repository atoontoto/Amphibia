#include "Tone3000Parsing.h"

#include "../../Network/HttpPolicy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace amphibia::providers::tone3000 {
namespace {

using Json = nlohmann::json;

template <typename T> ProviderResult<T> Failure(ProviderErrorCode code, const char* message)
{
  ProviderResult<T> result;
  result.error = {code, message};
  return result;
}

bool ValidateTree(const Json& value, const JsonLimits& limits, std::size_t depth, std::size_t& nodes)
{
  if (depth > limits.maximumDepth || ++nodes > limits.maximumNodes) return false;
  if (value.is_string() && value.get_ref<const std::string&>().size() > limits.maximumStringBytes) return false;
  if (value.is_array() || value.is_object())
    for (const auto& child : value)
      if (!ValidateTree(child, limits, depth + 1, nodes)) return false;
  return true;
}

bool ParseDocument(std::string_view body, const JsonLimits& limits, Json& output)
{
  if (body.empty() || body.size() > limits.maximumBytes) return false;
  output = Json::parse(body.begin(), body.end(), nullptr, false, true);
  std::size_t nodes = 0;
  return !output.is_discarded() && output.is_object() && ValidateTree(output, limits, 0, nodes);
}

std::optional<std::string> Id(const Json& value)
{
  if (value.is_string())
  {
    const auto text = value.get<std::string>();
    if (!text.empty() && text.size() <= 128) return text;
  }
  if (value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
  if (value.is_number_integer())
  {
    const auto integer = value.get<std::int64_t>();
    if (integer >= 0) return std::to_string(integer);
  }
  return std::nullopt;
}

std::optional<std::string> RequiredString(const Json& object, const char* key, std::size_t maximum = 8192)
{
  const auto iterator = object.find(key);
  if (iterator == object.end() || !iterator->is_string()) return std::nullopt;
  auto value = iterator->get<std::string>();
  if (value.empty() || value.size() > maximum) return std::nullopt;
  return value;
}

std::optional<std::string> OptionalString(const Json& object, const char* key, std::size_t maximum = 8192)
{
  const auto iterator = object.find(key);
  if (iterator == object.end() || iterator->is_null()) return std::nullopt;
  if (!iterator->is_string()) return std::nullopt;
  auto value = iterator->get<std::string>();
  if (value.size() > maximum) return std::nullopt;
  return value;
}

RemoteContentFormat ParseFormat(const std::string& value)
{
  if (value == "nam") return RemoteContentFormat::Nam;
  if (value == "ir") return RemoteContentFormat::ImpulseResponse;
  return RemoteContentFormat::Unknown;
}

RemoteGearRole ParseGear(const std::string& value)
{
  if (value == "amp") return RemoteGearRole::AmpHead;
  if (value == "amp-cab" || value == "full-rig") return RemoteGearRole::AmpAndCab;
  if (value == "pedal") return RemoteGearRole::Pedal;
  if (value == "cab" || value == "ir") return RemoteGearRole::Cabinet;
  if (value == "space") return RemoteGearRole::Space;
  if (value == "outboard") return RemoteGearRole::Outboard;
  if (value == "experimental") return RemoteGearRole::Experimental;
  return RemoteGearRole::Unknown;
}

RemoteNamArchitecture ParseArchitecture(const Json& value)
{
  if (value.is_string())
  {
    const auto text = value.get<std::string>();
    if (text == "1") return RemoteNamArchitecture::A1;
    if (text == "2") return RemoteNamArchitecture::A2;
  }
  if (value.is_number_integer())
  {
    const auto number = value.get<int>();
    if (number == 1) return RemoteNamArchitecture::A1;
    if (number == 2) return RemoteNamArchitecture::A2;
  }
  return RemoteNamArchitecture::Unknown;
}

bool PublicUrl(const std::string& value)
{
  return static_cast<bool>(network::ParseAndValidateUrl(value));
}

ProviderResult<RemoteModel> ParseModelValue(const Json& root, RemoteContentFormat expectedFormat)
{
  if (!root.is_object()) return Failure<RemoteModel>(ProviderErrorCode::InvalidResponse, "Model was not an object");
  const auto idIterator = root.find("id");
  const auto toneIterator = root.find("tone_id");
  const auto modelId = idIterator == root.end() ? std::nullopt : Id(*idIterator);
  const auto toneId = toneIterator == root.end() ? std::nullopt : Id(*toneIterator);
  const auto name = RequiredString(root, "name", 1024);
  const auto url = RequiredString(root, "model_url", 8192);
  if (!modelId || !toneId || !name || !url || !PublicUrl(*url))
    return Failure<RemoteModel>(ProviderErrorCode::InvalidResponse, "Model identity or download URL was invalid");
  RemoteModel model;
  model.providerToneId = *toneId;
  model.providerModelId = *modelId;
  model.name = *name;
  model.format = expectedFormat;
  if (const auto architecture = root.find("architecture_version"); architecture != root.end())
    model.architecture = ParseArchitecture(*architecture);
  model.downloadUrl = *url;
  if (const auto bytes = root.find("file_size"); bytes != root.end() && bytes->is_number_unsigned())
    model.declaredSize = bytes->get<std::uint64_t>();
  return {std::move(model), {}};
}

} // namespace

ProviderResult<credentials::OAuthTokenSet> ParseTokenResponse(
  std::string_view body, std::chrono::system_clock::time_point now, const JsonLimits& requestedLimits)
{
  JsonLimits limits = requestedLimits;
  limits.maximumBytes = (std::min)(limits.maximumBytes, static_cast<std::size_t>(64 * 1024));
  Json root;
  if (!ParseDocument(body, limits, root))
    return Failure<credentials::OAuthTokenSet>(ProviderErrorCode::InvalidJson, "Token response was invalid");
  const auto access = RequiredString(root, "access_token", 8192);
  const auto tokenType = RequiredString(root, "token_type", 32);
  const auto expiry = root.find("expires_in");
  if (!access || !tokenType || *tokenType != "bearer" || expiry == root.end() || !expiry->is_number_integer())
    return Failure<credentials::OAuthTokenSet>(ProviderErrorCode::InvalidResponse, "Token response fields were invalid");
  const auto seconds = expiry->get<std::int64_t>();
  if (seconds <= 0 || seconds > 31ll * 24 * 60 * 60)
    return Failure<credentials::OAuthTokenSet>(ProviderErrorCode::InvalidResponse, "Token expiry was invalid");
  credentials::OAuthTokenSet tokens;
  tokens.accessToken = *access;
  tokens.refreshToken = OptionalString(root, "refresh_token", 8192);
  tokens.tokenType = *tokenType;
  tokens.scope = OptionalString(root, "scope", 2048);
  tokens.accessTokenExpiry = now + std::chrono::seconds(seconds);
  return {std::move(tokens), {}};
}

ProviderResult<ProviderUser> ParseCurrentUser(std::string_view body, const JsonLimits& limits)
{
  Json root;
  if (!ParseDocument(body, limits, root)) return Failure<ProviderUser>(ProviderErrorCode::InvalidJson, "User response was invalid");
  const auto idIterator = root.find("id");
  const auto id = idIterator == root.end() ? std::nullopt : Id(*idIterator);
  const auto username = RequiredString(root, "username", 256);
  if (!id || !username) return Failure<ProviderUser>(ProviderErrorCode::InvalidResponse, "User identity was missing");
  ProviderUser user;
  user.providerUserId = *id;
  user.username = *username;
  user.displayName = OptionalString(root, "display_name", 512);
  user.avatarUrl = OptionalString(root, "avatar_url");
  user.profileUrl = OptionalString(root, "url");
  if ((user.avatarUrl && !PublicUrl(*user.avatarUrl)) || (user.profileUrl && !PublicUrl(*user.profileUrl)))
    return Failure<ProviderUser>(ProviderErrorCode::InvalidResponse, "User URL was invalid");
  return {std::move(user), {}};
}

ProviderResult<RemoteTone> ParseTone(std::string_view body, const JsonLimits& limits)
{
  Json root;
  if (!ParseDocument(body, limits, root)) return Failure<RemoteTone>(ProviderErrorCode::InvalidJson, "Tone response was invalid");
  const auto idIterator = root.find("id");
  const auto id = idIterator == root.end() ? std::nullopt : Id(*idIterator);
  const auto title = RequiredString(root, "title", 1024);
  const auto user = root.find("user");
  if (!id || !title || user == root.end() || !user->is_object())
    return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone identity was missing");
  const auto userIdIterator = user->find("id");
  const auto userId = userIdIterator == user->end() ? std::nullopt : Id(*userIdIterator);
  const auto username = RequiredString(*user, "username", 256);
  if (!userId || !username) return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone creator identity was missing");
  RemoteTone tone;
  tone.providerToneId = *id;
  tone.title = *title;
  tone.description = OptionalString(root, "description", 16384);
  tone.creator.providerUserId = *userId;
  tone.creator.username = *username;
  tone.creator.avatarUrl = OptionalString(*user, "avatar_url");
  tone.creator.profileUrl = OptionalString(*user, "url");
  if ((tone.creator.avatarUrl && !PublicUrl(*tone.creator.avatarUrl)) ||
      (tone.creator.profileUrl && !PublicUrl(*tone.creator.profileUrl)))
    return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone creator URL was invalid");
  if (const auto format = OptionalString(root, "format", 64)) tone.format = ParseFormat(*format);
  if (const auto gear = OptionalString(root, "gear", 64)) tone.gearRole = ParseGear(*gear);
  tone.licenseId = OptionalString(root, "license", 256);
  tone.licenseName = tone.licenseId;
  tone.sourcePageUrl = OptionalString(root, "url");
  if (tone.sourcePageUrl && !PublicUrl(*tone.sourcePageUrl))
    return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone source URL was invalid");
  if (const auto tags = root.find("tags"); tags != root.end() && !tags->is_null())
  {
    if (!tags->is_array() || tags->size() > limits.maximumTags)
      return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone tags exceeded limits");
    for (const auto& item : *tags)
    {
      const auto name = item.is_object() ? OptionalString(item, "name", 256) :
                        (item.is_string() && item.get_ref<const std::string&>().size() <= 256
                           ? std::optional<std::string>(item.get<std::string>()) : std::nullopt);
      if (name && !name->empty()) tone.tags.push_back(*name);
    }
  }
  if (const auto images = root.find("images"); images != root.end() && !images->is_null())
  {
    if (!images->is_array() || images->size() > limits.maximumImages)
      return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone images exceeded limits");
    for (const auto& item : *images)
    {
      if (!item.is_string() || !PublicUrl(item.get_ref<const std::string&>()))
        return Failure<RemoteTone>(ProviderErrorCode::InvalidResponse, "Tone image URL was invalid");
      tone.imageUrls.push_back(item.get<std::string>());
    }
  }
  return {std::move(tone), {}};
}

ProviderResult<RemoteModel> ParseModel(std::string_view body, RemoteContentFormat expectedFormat,
                                       const JsonLimits& limits)
{
  Json root;
  if (!ParseDocument(body, limits, root)) return Failure<RemoteModel>(ProviderErrorCode::InvalidJson, "Model response was invalid");
  return ParseModelValue(root, expectedFormat);
}

ProviderResult<std::vector<RemoteModel>> ParseModelList(std::string_view body,
                                                        RemoteContentFormat expectedFormat,
                                                        const JsonLimits& limits)
{
  auto page = ParseModelPage(body, expectedFormat, limits);
  if (!page) return Failure<std::vector<RemoteModel>>(page.error.code, page.error.userMessage.c_str());
  return {std::move(page.value->models), {}};
}

ProviderResult<RemoteModelPage> ParseModelPage(std::string_view body,
                                               RemoteContentFormat expectedFormat,
                                               const JsonLimits& limits)
{
  Json root;
  if (!ParseDocument(body, limits, root))
    return Failure<RemoteModelPage>(ProviderErrorCode::InvalidJson, "Model list response was invalid");
  const auto data = root.find("data");
  if (data == root.end() || !data->is_array() || data->size() > limits.maximumModels)
    return Failure<RemoteModelPage>(ProviderErrorCode::InvalidResponse, "Model list exceeded limits");
  const auto pageValue = root.find("page");
  const auto pageSizeValue = root.find("page_size");
  const auto totalValue = root.find("total");
  const auto totalPagesValue = root.find("total_pages");
  if (pageValue == root.end() || pageSizeValue == root.end() || totalValue == root.end() ||
      totalPagesValue == root.end() || !pageValue->is_number_unsigned() ||
      !pageSizeValue->is_number_unsigned() || !totalValue->is_number_unsigned() ||
      !totalPagesValue->is_number_unsigned())
    return Failure<RemoteModelPage>(ProviderErrorCode::InvalidResponse, "Model pagination metadata was missing");
  RemoteModelPage page;
  page.page = pageValue->get<std::size_t>();
  page.pageSize = pageSizeValue->get<std::size_t>();
  page.total = totalValue->get<std::size_t>();
  page.totalPages = totalPagesValue->get<std::size_t>();
  if (page.page == 0 || page.pageSize == 0 || page.pageSize > 300 || page.totalPages > 16 ||
      page.total > 4800 || (page.total == 0) != (page.totalPages == 0) ||
      (page.totalPages != 0 && page.page > page.totalPages) || data->size() > page.pageSize)
    return Failure<RemoteModelPage>(ProviderErrorCode::InvalidResponse, "Model pagination metadata was impossible");
  page.models.reserve(data->size());
  for (const auto& item : *data)
  {
    auto parsed = ParseModelValue(item, expectedFormat);
    if (!parsed) return Failure<RemoteModelPage>(parsed.error.code, parsed.error.userMessage.c_str());
    page.models.push_back(std::move(*parsed.value));
  }
  return {std::move(page), {}};
}

} // namespace amphibia::providers::tone3000
