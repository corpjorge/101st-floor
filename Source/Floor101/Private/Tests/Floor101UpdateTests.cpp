#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101Update.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

// Updates from signed GitHub releases (ADR-0039).
namespace
{
	using namespace Floor101::Update;

	// Signed by .NET's ECDsa.SignData, as Scripts/UpdateSigning.ps1 signs releases, with throwaway keys: Windows CNG must
	// accept exactly what PowerShell writes.
	const TCHAR* VectorKey = TEXT("8tXd34lpTr1x+HW/mPabaW8rVJR9UiVojT8J243PL9rzSA83cM3BOnk3VYrlV4ibdglkYJyqNKg8wCZZ7IMrvg==");
	const TCHAR* VectorMessage = TEXT("{\"product\":\"101st Floor\",\"version\":\"9.8.7\"}");
	const TCHAR* VectorSignature = TEXT("VG200d6nSLzerh83NRa2vJFIEE+RtHPUTiiVSgP4slRfFwqG9s0ry45UtluW+STXpnx94jopaOB0ZyOyDjzGWA==");

	// A release 9.8.7 whose "installer" is a line of text, signed with another throwaway key.
	const TCHAR* FeedKey = TEXT("KIvT8ASDgn0KeaeZM9ZG3vWrTuQp7OUxvUIWtLKOzgkfc5JycvoscubxMW/xrrarY464S4gbjKjvSl7WnCYDug==");
	const TCHAR* FeedManifest = TEXT("{\"product\":\"101st Floor\",\"version\":\"9.8.7\",\"setup\":{\"name\":\"101st-Floor-9.8.7-Setup.exe\",\"size\":26,")
		TEXT("\"sha256\":\"060327096abfc28b64d325aeab8ca5213058b4453c875aa3f8fdd7d84a55cf2f\"}}");
	const TCHAR* FeedSignature = TEXT("hNr88EsSirFszoV1D/FbOC0Q/qEdQHR1qv8d2JClRfqzhPrm8NorqxCe9kVleIOs9NK1WO6GZvc8Zd/c+pxMfg==");
	const TCHAR* FeedSetup = TEXT("101st Floor test installer");

	TArray<uint8> Bytes(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	FString Manifest(const FString& Name, const FString& Size, const FString& Sha256, const FString& Product = TEXT("101st Floor"),
		const FString& Version = TEXT("9.8.7"))
	{
		return FString::Printf(TEXT("{\"product\":\"%s\",\"version\":\"%s\",\"setup\":{\"name\":\"%s\",\"size\":%s,\"sha256\":\"%s\"}}"),
			*Product, *Version, *Name, *Size, *Sha256);
	}

	bool Save(const FString& Text, const FString& Path)
	{
		return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UpdateVersionsTest, "Floor101.Update.Versions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UpdateVersionsTest::RunTest(const FString& Parameters)
{
	auto Parsed = [](const TCHAR* Text) { const TOptional<FVersion> Version = FVersion::Parse(Text); return Version ? Version->ToString() : FString(TEXT("-")); };
	TestEqual(TEXT("plain"), Parsed(TEXT("0.3.0")), FString(TEXT("0.3.0")));
	TestEqual(TEXT("a tag"), Parsed(TEXT("v1.2.3")), FString(TEXT("1.2.3")));
	TestEqual(TEXT("pre-release and build marks ignored"), Parsed(TEXT("1.2.3-beta.1+7")), FString(TEXT("1.2.3")));
	TestEqual(TEXT("missing parts are zero"), Parsed(TEXT("0.3")), FString(TEXT("0.3.0")));
	for (const TCHAR* Bad : {TEXT(""), TEXT("1.2.3.4"), TEXT("1.x.0"), TEXT("-1.0.0"), TEXT("1..2"), TEXT("1.2.3a"), TEXT("1.2.1234567")})
	{
		TestFalse(FString::Printf(TEXT("'%s' is no version"), Bad), FVersion::Parse(Bad).IsSet());
	}
	const FVersion V029 = *FVersion::Parse(TEXT("0.2.9"));
	const FVersion V030 = *FVersion::Parse(TEXT("0.3.0"));
	const FVersion V0100 = *FVersion::Parse(TEXT("0.10.0"));
	TestTrue(TEXT("0.2.9 < 0.3.0"), V029 < V030);
	TestTrue(TEXT("numbers, not text: 0.3.0 < 0.10.0"), V030 < V0100);
	TestFalse(TEXT("not newer than itself"), V030 < V030);
	TestTrue(TEXT("1.0.0 is newer than 0.99.99"), *FVersion::Parse(TEXT("0.99.99")) < *FVersion::Parse(TEXT("1.0.0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UpdateManifestTest, "Floor101.Update.Manifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UpdateManifestTest::RunTest(const FString& Parameters)
{
	FString Why;
	const TOptional<FManifest> Good = ParseManifest(FeedManifest, Why);
	if (!TestTrue(TEXT("a release's manifest"), Good.IsSet())) { return false; }
	TestEqual(TEXT("version"), Good->Version, FString(TEXT("9.8.7")));
	TestEqual(TEXT("installer"), Good->SetupName, FString(TEXT("101st-Floor-9.8.7-Setup.exe")));
	TestEqual(TEXT("size"), Good->SetupSize, int64{26});
	const FString Hash = TEXT("060327096abfc28b64d325aeab8ca5213058b4453c875aa3f8fdd7d84a55cf2f");
	TestEqual(TEXT("an upper-case hash is read in lower case"), ParseManifest(Manifest(TEXT("a.exe"), TEXT("26"), Hash.ToUpper()), Why)->SetupSha256, Hash);
	const FString Bad[] = {
		TEXT("not json"),
		Manifest(TEXT("a.exe"), TEXT("26"), Hash, TEXT("Other app")),
		Manifest(TEXT("a.exe"), TEXT("26"), Hash, TEXT("101st Floor"), TEXT("latest")),
		Manifest(TEXT("..\\\\evil.exe"), TEXT("26"), Hash),
		Manifest(TEXT("C:evil.exe"), TEXT("26"), Hash),
		Manifest(TEXT("folder/evil.exe"), TEXT("26"), Hash),
		Manifest(TEXT("setup.msi"), TEXT("26"), Hash),
		Manifest(TEXT(".hidden.exe"), TEXT("26"), Hash),
		Manifest(TEXT("a.exe"), TEXT("0"), Hash),
		Manifest(TEXT("a.exe"), TEXT("-5"), Hash),
		Manifest(TEXT("a.exe"), TEXT("26.5"), Hash),
		Manifest(TEXT("a.exe"), TEXT("26"), Hash.Left(63)),
		Manifest(TEXT("a.exe"), TEXT("26"), Hash.Left(63) + TEXT("g")),
	};
	for (const FString& Json : Bad)
	{
		Why.Empty();
		TestFalse(FString::Printf(TEXT("refused: %s"), *Json), ParseManifest(Json, Why).IsSet());
		TestFalse(TEXT("with a reason"), Why.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UpdateSignatureTest, "Floor101.Update.Signature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UpdateSignatureTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("CNG verifies what .NET signed"), VerifySignature(Bytes(VectorMessage), VectorSignature, VectorKey));
	TestFalse(TEXT("another message"), VerifySignature(Bytes(FString(VectorMessage).Replace(TEXT("9.8.7"), TEXT("9.8.8"))), VectorSignature, VectorKey));
	FString Altered = VectorSignature;
	Altered[10] = Altered[10] == TEXT('A') ? TEXT('B') : TEXT('A');
	TestFalse(TEXT("an altered signature"), VerifySignature(Bytes(VectorMessage), Altered, VectorKey));
	TestFalse(TEXT("another key"), VerifySignature(Bytes(VectorMessage), VectorSignature, FeedKey));
	TestFalse(TEXT("not Base64"), VerifySignature(Bytes(VectorMessage), TEXT("***"), VectorKey));
	TestFalse(TEXT("too short"), VerifySignature(Bytes(VectorMessage), TEXT("AAAA"), VectorKey));
	TestFalse(TEXT("no key"), VerifySignature(Bytes(VectorMessage), VectorSignature, TEXT("")));
	TArray<uint8> Trusted;
	TestTrue(TEXT("the project's key is an X||Y point"), FBase64::Decode(TrustedPublicKey(), Trusted) && Trusted.Num() == 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UpdateReleaseTest, "Floor101.Update.Release",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UpdateReleaseTest::RunTest(const FString& Parameters)
{
	const TOptional<FRelease> Release = ParseRelease(TEXT("{\"html_url\":\"https://github.com/o/r/releases/tag/v0.3.0\",\"tag_name\":\"v0.3.0\",")
		TEXT("\"assets\":[{\"name\":\"a.json\",\"browser_download_url\":\"https://example.invalid/a.json\"},{\"name\":\"no-address\"}]}"));
	if (!TestTrue(TEXT("GitHub's latest release"), Release.IsSet())) { return false; }
	TestEqual(TEXT("its page"), Release->PageUrl, FString(TEXT("https://github.com/o/r/releases/tag/v0.3.0")));
	TestEqual(TEXT("assets with an address"), Release->Assets.Num(), 1);
	TestEqual(TEXT("by name"), Release->Assets.FindRef(TEXT("a.json")), FString(TEXT("https://example.invalid/a.json")));
	TestFalse(TEXT("not JSON"), ParseRelease(TEXT("<html>")).IsSet());
	TestFalse(TEXT("no assets (an error from the API)"), ParseRelease(TEXT("{\"message\":\"Not Found\"}")).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UpdateLocalFeedTest, "Floor101.Update.LocalFeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UpdateLocalFeedTest::RunTest(const FString& Parameters)
{
	// The whole flow on local files: check, verify, compare, download, verify the installer, stop before running it.
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Update"), FGuid::NewGuid().ToString()));
	const FString Downloaded = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("101st Floor"), TEXT("Updates"), TEXT("101st-Floor-9.8.7-Setup.exe"));
	FUpdater& Updater = FUpdater::Get();
	ON_SCOPE_EXIT
	{
		Updater.Reset();
		IFileManager::Get().DeleteDirectory(*Folder, false, true);
		IFileManager::Get().Delete(*Downloaded, false, true, true);
	};
	const FString Setup = FPaths::Combine(Folder, TEXT("101st-Floor-9.8.7-Setup.exe"));
	const FString ManifestPath = FPaths::Combine(Folder, ManifestName);
	const FString SignaturePath = FPaths::Combine(Folder, SignatureName);
	auto Release = [&](bool bSigned)
	{
		FString Assets = FString::Printf(TEXT("{\"name\":\"%s\",\"browser_download_url\":\"%s\"},{\"name\":\"%s\",\"browser_download_url\":\"%s\"}"),
			ManifestName, *ManifestPath, TEXT("101st-Floor-9.8.7-Setup.exe"), *Setup);
		if (bSigned) { Assets += FString::Printf(TEXT(",{\"name\":\"%s\",\"browser_download_url\":\"%s\"}"), SignatureName, *SignaturePath); }
		const FString Path = FPaths::Combine(Folder, bSigned ? TEXT("release.json") : TEXT("unsigned.json"));
		Save(FString::Printf(TEXT("{\"html_url\":\"https://example.invalid/v9.8.7\",\"assets\":[%s]}"), *Assets), Path);
		return Path;
	};
	auto Start = [&](const FString& Feed, const TCHAR* Version, const TCHAR* Key)
	{
		Updater.Reset();
		Updater.SetFeed(Feed);
		Updater.SetCurrentVersion(Version);
		Updater.SetTrustedKey(Key);
		Updater.SetDryRun(true);
		Updater.Check(); // local files answer at once
	};
	TestTrue(TEXT("installer"), Save(FeedSetup, Setup));
	TestTrue(TEXT("manifest"), Save(FeedManifest, ManifestPath));
	TestTrue(TEXT("signature"), Save(FeedSignature, SignaturePath));
	const FString Feed = Release(true);

	Start(Feed, TEXT("0.3.0"), FeedKey);
	if (!TestTrue(TEXT("a newer, signed version is available"), Updater.GetState() == EState::Available)) { return false; }
	TestEqual(TEXT("its version"), Updater.GetAvailable().Version, FString(TEXT("9.8.7")));
	Updater.Download();
	TestTrue(TEXT("its installer is downloaded and matches"), Updater.GetState() == EState::Ready && IFileManager::Get().FileExists(*Downloaded));
	TestFalse(TEXT("a dry run never starts it"), Updater.LaunchInstaller());
	TestTrue(TEXT("still ready"), Updater.GetState() == EState::Ready);

	Start(Feed, TEXT("9.8.7"), FeedKey);
	TestTrue(TEXT("the same version is up to date"), Updater.GetState() == EState::UpToDate);
	Start(Feed, TEXT("10.0.0"), FeedKey);
	TestTrue(TEXT("an older one is never offered"), Updater.GetState() == EState::UpToDate);

	AddExpectedMessagePlain(TEXT("F101Update: La versión publicada no tiene una firma válida"), ELogVerbosity::Warning);
	Start(Feed, TEXT("0.3.0"), VectorKey);
	TestTrue(TEXT("signed with another key: refused"), Updater.GetState() == EState::Failed);

	AddExpectedMessagePlain(TEXT("F101Update: La última versión publicada no trae su descripción firmada"), ELogVerbosity::Warning);
	Start(Release(false), TEXT("0.3.0"), FeedKey);
	TestTrue(TEXT("without a signature: refused"), Updater.GetState() == EState::Failed);

	AddExpectedMessagePlain(TEXT("F101Update: El instalador descargado no coincide"), ELogVerbosity::Warning);
	IFileManager::Get().Delete(*Downloaded, false, true, true);
	TestTrue(TEXT("another installer"), Save(TEXT("101st Floor fake installer"), Setup)); // same size, other bytes
	Start(Feed, TEXT("0.3.0"), FeedKey);
	TestTrue(TEXT("still available"), Updater.GetState() == EState::Available);
	Updater.Download();
	TestTrue(TEXT("an installer that does not match is refused"), Updater.GetState() == EState::Failed);
	TestFalse(TEXT("and removed"), IFileManager::Get().FileExists(*Downloaded) || IFileManager::Get().FileExists(*(Downloaded + TEXT(".part"))));
	return true;
}

#endif
