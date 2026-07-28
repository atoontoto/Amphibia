#include "../../NeuralAmpModeler/Providers/IHostedSelectionCallbackReceiver.h"
#include "../../NeuralAmpModeler/Providers/IRemoteToneProvider.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000Configuration.h"
#include "../../NeuralAmpModeler/Providers/Tone3000/Tone3000HostedSelectionCoordinator.h"
#include "../../NeuralAmpModeler/UI/Tone3000SelectionPanelState.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace amphibia;

namespace {

int gPassed{};

void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

template <typename Function> void Test(const char* name, Function function)
{
  try { function(); ++gPassed; std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; std::exit(1); }
}

providers::tone3000::Tone3000Configuration DevelopmentConfiguration(const std::string& account)
{
  providers::tone3000::Tone3000Configuration result;
  result.clientId = "test-publishable-client";
  result.authorizationEndpoint = "https://provider.test/api/v1/oauth/authorize";
  result.tokenEndpoint = "https://provider.test/api/v1/oauth/token";
  result.apiBaseUrl = "https://provider.test";
  result.redirectUri = "http://127.0.0.1:43821/oauth/callback";
  result.kind = providers::tone3000::Tone3000ConfigurationKind::Test;
  result.callbackStrategy = providers::tone3000::Tone3000CallbackStrategy::DevelopmentMock;
  result.allowTestHttp = true;
  result.credentialServiceNamespace = "org.amphibiaaudio.Amphibia.TONE3000.m5";
  result.credentialAccount = account;
  return result;
}

class FakeReceiver final : public providers::IHostedSelectionCallbackReceiver
{
public:
  providers::ProviderError Start(const std::string& redirect, std::chrono::steady_clock::time_point deadline,
                                 providers::HostedCallbackCompletion callback) override
  {
    ++starts;
    if (active) return {providers::ProviderErrorCode::Superseded, "receiver busy"};
    exactRedirect = redirect;
    expires = deadline;
    completion = std::move(callback);
    active = true;
    return {};
  }
  void Cancel() override { active = false; ++cancels; }
  void Shutdown() override { active = false; completion = {}; ++shutdowns; }
  bool IsActive() const override { return active; }
  void Complete(std::string callbackUri)
  {
    auto callback = completion;
    active = false;
    if (callback) callback({std::move(callbackUri), {}});
  }
  void Fail(providers::ProviderErrorCode code)
  {
    auto callback = completion;
    active = false;
    if (callback) callback({std::nullopt, {code, "callback failed"}});
  }
  providers::HostedCallbackCompletion completion;
  std::chrono::steady_clock::time_point expires;
  std::string exactRedirect;
  bool active{};
  int starts{};
  int cancels{};
  int shutdowns{};
};

class FakeProvider final : public providers::IRemoteToneProvider
{
public:
  providers::ProviderConnectionState GetConnectionState() const override
  { return providers::ProviderConnectionState::Connected; }
  providers::ProviderTaskStatus GetTaskStatus() const override { return {}; }
  void BeginAuthorization(const providers::AuthorizationRequest& request,
                          providers::AuthorizationCallback callback) override
  { lastAuthorization = request; authorization = std::move(callback); ++authorizationStarts; }
  void CancelAuthorization() override
  {
    ++authorizationCancels;
    auto callback = std::move(authorization);
    if (callback) callback({std::nullopt, {providers::ProviderErrorCode::Cancelled, "cancelled"}});
  }
  void Disconnect() override { ++disconnects; }
  void FetchCurrentUser(providers::ProviderUserCallback callback) override
  { callback({std::nullopt, {providers::ProviderErrorCode::NotFound, "unused"}}); }
  void FetchTone(const std::string& toneId, providers::ToneCallback callback) override
  { ++toneFetches; lastToneId = toneId; callback(toneResult); }
  void FetchModelsForTone(const std::string& toneId, providers::RemoteContentFormat format,
                          std::optional<providers::RemoteNamArchitecture> architecture,
                          providers::ModelListCallback callback) override
  {
    ++modelFetches; lastToneId = toneId; listedFormat = format; listedArchitecture = architecture;
    callback(modelResult);
  }
  void FetchExactModel(const std::string& modelId, providers::RemoteContentFormat format,
                       providers::ModelCallback callback) override
  { ++exactFetches; exactModelId = modelId; exactFormat = format; callback(exactResult); }
  void BeginModelDownload(const providers::RemoteModel& model, const providers::DownloadDestination&,
                          providers::DownloadCallback callback) override
  { ++downloads; downloadedModelId = model.providerModelId; callback(downloadResult); }
  void CompleteAuthorizationCallback(const std::string& callbackUri) override
  {
    completedCallback = callbackUri;
    auto callback = std::move(authorization);
    if (callback) callback(authorizationResult);
  }
  providers::ProviderError ReopenAuthorizationBrowser() override { ++reopens; return reopenError; }
  void CancelCurrentOperation() override { ++operationCancels; CancelAuthorization(); }
  void Shutdown() override { ++shutdowns; authorization = {}; }

  providers::AuthorizationRequest lastAuthorization;
  providers::AuthorizationCallback authorization;
  providers::ProviderResult<providers::AuthorizationResult> authorizationResult;
  providers::ProviderResult<providers::RemoteTone> toneResult;
  providers::ProviderResult<std::vector<providers::RemoteModel>> modelResult;
  providers::ProviderResult<providers::RemoteModel> exactResult;
  providers::ProviderResult<providers::DownloadResult> downloadResult;
  providers::ProviderError reopenError;
  std::optional<providers::RemoteNamArchitecture> listedArchitecture;
  providers::RemoteContentFormat listedFormat{providers::RemoteContentFormat::Unknown};
  providers::RemoteContentFormat exactFormat{providers::RemoteContentFormat::Unknown};
  std::string completedCallback, lastToneId, exactModelId, downloadedModelId;
  int authorizationStarts{}, authorizationCancels{}, disconnects{}, toneFetches{}, modelFetches{};
  int exactFetches{}, downloads{}, reopens{}, operationCancels{}, shutdowns{};
};

providers::RemoteTone Tone(providers::RemoteContentFormat format)
{
  providers::RemoteTone tone;
  tone.providerToneId = "42";
  tone.title = "Exact Pack";
  tone.creator.providerUserId = "creator-1";
  tone.creator.username = "maker";
  tone.format = format;
  tone.gearRole = format == providers::RemoteContentFormat::Nam ? providers::RemoteGearRole::AmpHead :
                                                                  providers::RemoteGearRole::Cabinet;
  tone.sourcePageUrl = "https://www.tone3000.com/tones/42";
  return tone;
}

providers::RemoteModel Model(const char* id, providers::RemoteContentFormat format,
                             providers::RemoteNamArchitecture architecture,
                             const char* name = "Variant")
{
  providers::RemoteModel model;
  model.providerToneId = "42";
  model.providerModelId = id;
  model.name = name;
  model.format = format;
  model.architecture = architecture;
  model.relativePath = std::string("Channel 1/") + name;
  model.downloadUrl = std::string("https://provider.test/models/") + id;
  return model;
}

void CompleteHostedSelection(FakeReceiver& receiver)
{
  receiver.Complete("http://127.0.0.1:43821/oauth/callback?code=secret&state=secret&tone_id=42");
}

} // namespace

int main()
{
  Test("registration validator fails closed", [] {
    const auto unconfigured = providers::tone3000::ValidateTone3000Configuration({});
    Require(unconfigured.valid && !unconfigured.providerUsable &&
            unconfigured.state == providers::tone3000::Tone3000RegistrationState::Unconfigured,
            "unconfigured state");
    auto production = DevelopmentConfiguration("configuration");
    production.kind = providers::tone3000::Tone3000ConfigurationKind::Production;
    production.callbackStrategy = providers::tone3000::Tone3000CallbackStrategy::DevelopmentMock;
    const auto invalid = providers::tone3000::ValidateTone3000Configuration(production);
    Require(!invalid.valid && !invalid.providerUsable,
            "development callback was accepted as production");
  });

  Test("unconfigured selection is inert", [] {
    FakeProvider provider; FakeReceiver receiver;
    providers::tone3000::Tone3000HostedSelectionCoordinator coordinator({}, provider, receiver);
    const auto error = coordinator.Begin(providers::HostedSelectionKind::NamA1AndCustom);
    Require(error.code == providers::ProviderErrorCode::Unconfigured && receiver.starts == 0 &&
            provider.authorizationStarts == 0, "unconfigured build performed provider work");
  });

  Test("NAM hosted selection lists exact variants before explicit download", [] {
    FakeProvider provider; FakeReceiver receiver;
    provider.authorizationResult = {providers::AuthorizationResult{std::string("42"), false}, {}};
    provider.toneResult = {Tone(providers::RemoteContentFormat::Nam), {}};
    const auto a1 = Model("a1", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A1, "Gain 2");
    const auto a2 = Model("a2", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A2, "Gain 5");
    const auto unknown = Model("custom", providers::RemoteContentFormat::Nam,
                               providers::RemoteNamArchitecture::Unknown, "Custom");
    provider.modelResult = {std::vector<providers::RemoteModel>{a2, unknown, a1}, {}};
    provider.exactResult = {a1, {}};
    provider.downloadResult = {providers::DownloadResult{{}, 100, std::string(64, 'a')}, {}};
    std::uint64_t activationTask{}; std::string activationHash;
    providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
      DevelopmentConfiguration("nam-flow"), provider, receiver, {},
      [&](std::uint64_t task, const std::string& hash, providers::HostedSelectionKind) {
        activationTask = task; activationHash = hash;
      });
    Require(!coordinator.Begin(providers::HostedSelectionKind::NamA1AndCustom), "begin failed");
    Require(receiver.IsActive() && provider.lastAuthorization.purpose == providers::AuthorizationPurpose::SelectTone &&
            provider.lastAuthorization.format == providers::RemoteContentFormat::Nam &&
            provider.lastAuthorization.gearFilters == std::vector<std::string>({"amp", "amp-cab"}) &&
            !provider.lastAuthorization.architecture, "NAM hosted constraints");
    Require(provider.downloads == 0, "download began before callback and exact choice");
    CompleteHostedSelection(receiver);
    auto snapshot = coordinator.Snapshot();
    Require(snapshot.stage == providers::HostedSelectionStage::Ready && snapshot.variants.size() == 3 &&
            provider.toneFetches == 1 && provider.modelFetches == 1 && provider.downloads == 0,
            "exact metadata/list state");
    const auto compatible = std::find_if(snapshot.variants.begin(), snapshot.variants.end(), [](const auto& variant) {
      return variant.model.providerModelId == "a1";
    });
    Require(compatible != snapshot.variants.end() &&
            compatible->compatibility == providers::VariantCompatibility::Compatible,
            "A1 compatibility");
    Require(std::count_if(snapshot.variants.begin(), snapshot.variants.end(), [](const auto& variant) {
      return variant.compatibility != providers::VariantCompatibility::Compatible;
    }) == 2, "unsupported variants were falsely enabled");
    const auto compatibleIndex = static_cast<std::size_t>(std::distance(snapshot.variants.begin(), compatible));
    Require(!coordinator.SelectVariant(compatibleIndex), "select exact variant");
    Require(!coordinator.DownloadSelected(true) && provider.exactFetches == 1 && provider.downloads == 1 &&
            activationTask == snapshot.taskId && activationHash.size() == 64,
            "explicit exact download/audition pipeline");
    coordinator.NotifyActivation(snapshot.taskId, "a1", true);
    snapshot = coordinator.Snapshot();
    Require(snapshot.stage == providers::HostedSelectionStage::Active &&
            snapshot.variants[compatibleIndex].active, "activation was not independently confirmed");
  });

  Test("A2 and IR hosted constraints are documented values", [] {
    {
      FakeProvider provider; FakeReceiver receiver;
      providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
        DevelopmentConfiguration("a2-constraints"), provider, receiver);
      Require(!coordinator.Begin(providers::HostedSelectionKind::NamA2), "A2 begin");
      Require(provider.lastAuthorization.architecture == providers::RemoteNamArchitecture::A2 &&
              provider.lastAuthorization.format == providers::RemoteContentFormat::Nam,
              "A2 constraints");
      coordinator.Cancel();
    }
    {
      FakeProvider provider; FakeReceiver receiver;
      providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
        DevelopmentConfiguration("ir-constraints"), provider, receiver);
      Require(!coordinator.Begin(providers::HostedSelectionKind::ImpulseResponse), "IR begin");
      Require(provider.lastAuthorization.gearFilters == std::vector<std::string>({"cab"}) &&
              provider.lastAuthorization.format == providers::RemoteContentFormat::ImpulseResponse &&
              !provider.lastAuthorization.architecture, "IR constraints");
      coordinator.Cancel();
    }
  });

  Test("already downloaded variant still requires explicit audition", [] {
    FakeProvider provider; FakeReceiver receiver;
    provider.authorizationResult = {providers::AuthorizationResult{std::string("42"), false}, {}};
    provider.toneResult = {Tone(providers::RemoteContentFormat::Nam), {}};
    provider.modelResult = {std::vector<providers::RemoteModel>{
      Model("cached", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A1)}, {}};
    bool activated = false;
    providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
      DevelopmentConfiguration("cached"), provider, receiver,
      [](const auto&) { return std::optional<std::string>(std::string(64, 'b')); },
      [&](auto, const auto&, auto) { activated = true; });
    Require(!coordinator.Begin(providers::HostedSelectionKind::NamA1AndCustom), "begin cached");
    CompleteHostedSelection(receiver);
    Require(!activated && provider.downloads == 0, "cached variant auto-activated");
    Require(!coordinator.DownloadSelected(true) && activated && provider.exactFetches == 0 &&
            coordinator.Snapshot().stage == providers::HostedSelectionStage::Preparing,
            "cached audition did not use explicit preparing stage");
  });

  Test("cancel and stale callback cannot advance selection", [] {
    FakeProvider provider; FakeReceiver receiver;
    providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
      DevelopmentConfiguration("cancel"), provider, receiver);
    Require(!coordinator.Begin(providers::HostedSelectionKind::ImpulseResponse), "begin cancel");
    const auto stale = receiver.completion;
    coordinator.Cancel();
    if (stale) stale({std::string("http://127.0.0.1:43821/oauth/callback?code=x&state=y&tone_id=42"), {}});
    Require(coordinator.Snapshot().stage == providers::HostedSelectionStage::Cancelled &&
            provider.toneFetches == 0 && provider.operationCancels == 1,
            "cancelled task accepted stale callback");
  });

  Test("callback failure keeps its safe terminal reason", [] {
    FakeProvider provider; FakeReceiver receiver;
    providers::tone3000::Tone3000HostedSelectionCoordinator coordinator(
      DevelopmentConfiguration("callback-failure"), provider, receiver);
    Require(!coordinator.Begin(providers::HostedSelectionKind::NamA2), "begin callback failure");
    receiver.Fail(providers::ProviderErrorCode::RequestTimeout);
    const auto snapshot = coordinator.Snapshot();
    Require(snapshot.stage == providers::HostedSelectionStage::Failed && snapshot.error &&
            snapshot.error->code == providers::ProviderErrorCode::RequestTimeout && provider.toneFetches == 0,
            "authorization cancellation replaced the callback failure");
  });

  Test("terminal hosted-selection panel state is explicitly closeable", [] {
    providers::HostedSelectionSnapshot snapshot;
    snapshot.stage = providers::HostedSelectionStage::WaitingForBrowser;
    Require(!ui::IsTone3000SelectionPanelCloseState(snapshot),
            "waiting selection incorrectly exposed a terminal close state");
    snapshot.stage = providers::HostedSelectionStage::Cancelled;
    Require(ui::IsTone3000SelectionPanelCloseState(snapshot),
            "cancelled selection did not expose a terminal close state");
    snapshot.stage = providers::HostedSelectionStage::Failed;
    Require(ui::IsTone3000SelectionPanelCloseState(snapshot),
            "failed selection did not expose a terminal close state");
    snapshot.stage = providers::HostedSelectionStage::FetchingTone;
    Require(!ui::IsTone3000SelectionPanelCloseState(snapshot),
            "active no-tone selection incorrectly exposed a terminal close state");
    snapshot.tone = Tone(providers::RemoteContentFormat::Nam);
    snapshot.stage = providers::HostedSelectionStage::Ready;
    Require(!ui::IsTone3000SelectionPanelCloseState(snapshot),
            "ready selection incorrectly exposed a terminal close state");
  });

  Test("pack filter grouping and downloaded navigation are local", [] {
    providers::HostedSelectionSnapshot snapshot;
    providers::HostedVariant first, second, third;
    first.model = Model("1", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A1, "Gain 2");
    second.model = Model("2", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A1, "Gain 5");
    third.model = Model("3", providers::RemoteContentFormat::Nam, providers::RemoteNamArchitecture::A1, "Bright");
    first.compatibility = second.compatibility = third.compatibility = providers::VariantCompatibility::Compatible;
    snapshot.variants = {first, second, third};
    const auto filtered = providers::tone3000::Tone3000HostedSelectionCoordinator::FilteredVariantIndices(
      snapshot, "gain", 0, 12);
    Require(filtered == std::vector<std::size_t>({0, 1}) &&
            providers::tone3000::Tone3000HostedSelectionCoordinator::GroupLabel(snapshot.variants[0]) == "Channel 1",
            "in-pack filter/grouping");
  });

  Test("selection lease prevents concurrent browser ownership", [] {
    FakeProvider firstProvider, secondProvider; FakeReceiver firstReceiver, secondReceiver;
    auto configuration = DevelopmentConfiguration("lease");
    providers::tone3000::Tone3000HostedSelectionCoordinator first(configuration, firstProvider, firstReceiver);
    providers::tone3000::Tone3000HostedSelectionCoordinator second(configuration, secondProvider, secondReceiver);
    Require(!first.Begin(providers::HostedSelectionKind::NamA1AndCustom), "first lease");
    const auto busy = second.Begin(providers::HostedSelectionKind::NamA1AndCustom);
    Require(busy.code == providers::ProviderErrorCode::Superseded && secondReceiver.starts == 0,
            "concurrent selection was not rejected");
    first.Cancel();
    Require(!second.Begin(providers::HostedSelectionKind::NamA1AndCustom), "lease was not released");
    second.Cancel();
  });

  std::cout << "Milestone 5 selection tests passed: " << gPassed << '\n';
  return 0;
}
