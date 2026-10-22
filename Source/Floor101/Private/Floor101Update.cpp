#include "Floor101Update.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Floor101.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "Floor101Update"

namespace Floor101::Update
{
	namespace
	{
		// Scripts/UpdateSigningKey.ps1: the public half of the key that signs every release (ADR-0039). Release.ps1 refuses
		// to sign with any other key.
		const TCHAR* UpdatePublicKey = TEXT("xwlRKEkzjgsD1t9Wk+t0EYiagaLRGLfw6ufXyF+Wcd7wLKmcHl3QvXi+hpceRxMX/7jvwzx77K78jZdtNSN7+Q==");
		const TCHAR* UpdateSection = TEXT("Floor101.Update");
		const TCHAR* UpdateProduct = TEXT("101st Floor");
		constexpr int64 MaxSetupBytes = 8ll * 1024 * 1024 * 1024;

		bool IsRemote(const FString& Address)
		{
			return Address.StartsWith(TEXT("https://")) || Address.StartsWith(TEXT("http://"));
		}

		FString Utf8(const TArray<uint8>& Bytes)
		{
			const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			return FString(Text.Length(), Text.Get());
		}

		TSharedPtr<FJsonObject> ReadObject(const FString& Json)
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
		}

		bool IsDigits(const FString& Text)
		{
			if (Text.IsEmpty() || Text.Len() > 6) { return false; }
			for (const TCHAR Character : Text) { if (!FChar::IsDigit(Character)) { return false; } }
			return true;
		}

#if PLATFORM_WINDOWS
		// SHA-256 with Windows CNG, fed in pieces.
		class FSha256
		{
		public:
			FSha256()
			{
				bValid = BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
					BCRYPT_SUCCESS(BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0));
			}
			~FSha256()
			{
				if (Hash) { BCryptDestroyHash(Hash); }
				if (Algorithm) { BCryptCloseAlgorithmProvider(Algorithm, 0); }
			}
			void Add(const uint8* Data, int64 Size)
			{
				while (bValid && Size > 0)
				{
					const ULONG Piece = static_cast<ULONG>(FMath::Min<int64>(Size, 1 << 30));
					bValid = BCRYPT_SUCCESS(BCryptHashData(Hash, const_cast<PUCHAR>(Data), Piece, 0));
					Data += Piece;
					Size -= Piece;
				}
			}
			bool Finish(uint8 (&Digest)[32])
			{
				return bValid && BCRYPT_SUCCESS(BCryptFinishHash(Hash, Digest, 32, 0));
			}

		private:
			BCRYPT_ALG_HANDLE Algorithm = nullptr;
			BCRYPT_HASH_HANDLE Hash = nullptr;
			bool bValid = false;
		};
#endif
	}

	TOptional<FVersion> FVersion::Parse(const FString& Text)
	{
		FString Core = Text.TrimStartAndEnd();
		if (Core.StartsWith(TEXT("v"), ESearchCase::IgnoreCase)) { Core.RightChopInline(1); }
		int32 Cut = INDEX_NONE;
		for (const TCHAR Mark : {TEXT('-'), TEXT('+')})
		{
			int32 At = INDEX_NONE;
			if (Core.FindChar(Mark, At)) { Cut = Cut == INDEX_NONE ? At : FMath::Min(Cut, At); }
		}
		if (Cut != INDEX_NONE) { Core.LeftInline(Cut); }
		TArray<FString> Parts;
		Core.ParseIntoArray(Parts, TEXT("."), false);
		if (Parts.IsEmpty() || Parts.Num() > 3) { return {}; }
		FVersion Version;
		int32* Numbers[] = {&Version.Major, &Version.Minor, &Version.Patch};
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			if (!IsDigits(Parts[Index])) { return {}; }
			*Numbers[Index] = FCString::Atoi(*Parts[Index]);
		}
		return Version;
	}

	bool FVersion::operator<(const FVersion& Other) const
	{
		if (Major != Other.Major) { return Major < Other.Major; }
		if (Minor != Other.Minor) { return Minor < Other.Minor; }
		return Patch < Other.Patch;
	}

	FString FVersion::ToString() const
	{
		return FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Patch);
	}

	TOptional<FManifest> ParseManifest(const FString& Json, FString& OutError)
	{
		const TSharedPtr<FJsonObject> Root = ReadObject(Json);
		FString Name;
		const TSharedPtr<FJsonObject>* Setup = nullptr;
		FManifest Manifest;
		double Size = 0.0;
		if (!Root || !Root->TryGetStringField(TEXT("product"), Name) || Name != UpdateProduct)
		{
			OutError = TEXT("not a 101st Floor update manifest");
			return {};
		}
		if (!Root->TryGetStringField(TEXT("version"), Manifest.Version) || !FVersion::Parse(Manifest.Version))
		{
			OutError = TEXT("no valid version");
			return {};
		}
		if (!Root->TryGetObjectField(TEXT("setup"), Setup) || !(*Setup)->TryGetStringField(TEXT("name"), Manifest.SetupName) ||
			!(*Setup)->TryGetNumberField(TEXT("size"), Size) || !(*Setup)->TryGetStringField(TEXT("sha256"), Manifest.SetupSha256))
		{
			OutError = TEXT("no installer described");
			return {};
		}
		// A plain file name: it is created in the updates folder and nowhere else.
		const FString& File = Manifest.SetupName;
		if (!File.EndsWith(TEXT(".exe"), ESearchCase::IgnoreCase) || File.StartsWith(TEXT(".")) || File.Contains(TEXT("..")) ||
			File.Contains(TEXT("/")) || File.Contains(TEXT("\\")) || File.Contains(TEXT(":")) || File.Len() > 128)
		{
			OutError = TEXT("the installer name is not a plain .exe file name");
			return {};
		}
		Manifest.SetupSize = static_cast<int64>(Size);
		if (Manifest.SetupSize <= 0 || static_cast<double>(Manifest.SetupSize) != Size || Manifest.SetupSize > MaxSetupBytes)
		{
			OutError = TEXT("the installer size is not valid");
			return {};
		}
		Manifest.SetupSha256.ToLowerInline();
		bool bHex = Manifest.SetupSha256.Len() == 64;
		for (const TCHAR Character : Manifest.SetupSha256) { bHex = bHex && FChar::IsHexDigit(Character); }
		if (!bHex)
		{
			OutError = TEXT("the installer hash is not SHA-256");
			return {};
		}
		return Manifest;
	}

	TOptional<FRelease> ParseRelease(const FString& Json)
	{
		const TSharedPtr<FJsonObject> Root = ReadObject(Json);
		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		if (!Root || !Root->TryGetArrayField(TEXT("assets"), Assets))
		{
			return {};
		}
		FRelease Release;
		Root->TryGetStringField(TEXT("html_url"), Release.PageUrl);
		for (const TSharedPtr<FJsonValue>& Value : *Assets)
		{
			const TSharedPtr<FJsonObject>* Asset = nullptr;
			FString Name;
			FString Address;
			if (Value.IsValid() && Value->TryGetObject(Asset) && (*Asset)->TryGetStringField(TEXT("name"), Name) &&
				(*Asset)->TryGetStringField(TEXT("browser_download_url"), Address))
			{
				Release.Assets.Add(Name, Address);
			}
		}
		return Release;
	}

	bool VerifySignature(TConstArrayView<uint8> Bytes, const FString& SignatureBase64, const FString& PublicKeyBase64)
	{
#if PLATFORM_WINDOWS
		TArray<uint8> Key;
		TArray<uint8> Signature;
		uint8 Digest[32];
		if (!FBase64::Decode(PublicKeyBase64.TrimStartAndEnd(), Key) || Key.Num() != 64 ||
			!FBase64::Decode(SignatureBase64.TrimStartAndEnd(), Signature) || Signature.Num() != 64)
		{
			return false;
		}
		FSha256 Hash;
		Hash.Add(Bytes.GetData(), Bytes.Num());
		if (!Hash.Finish(Digest)) { return false; }
		BCRYPT_ALG_HANDLE Algorithm = nullptr;
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0))) { return false; }
		ON_SCOPE_EXIT { BCryptCloseAlgorithmProvider(Algorithm, 0); };
		const BCRYPT_ECCKEY_BLOB Header{BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32};
		TArray<uint8> Blob;
		Blob.Append(reinterpret_cast<const uint8*>(&Header), sizeof(Header));
		Blob.Append(Key);
		BCRYPT_KEY_HANDLE PublicKey = nullptr;
		if (!BCRYPT_SUCCESS(BCryptImportKeyPair(Algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &PublicKey, Blob.GetData(), Blob.Num(), 0)))
		{
			return false;
		}
		ON_SCOPE_EXIT { BCryptDestroyKey(PublicKey); };
		return BCryptVerifySignature(PublicKey, nullptr, Digest, sizeof(Digest), Signature.GetData(), Signature.Num(), 0) == 0;
#else
		return false; // Windows only (ADR-0004)
#endif
	}

	const TCHAR* TrustedPublicKey()
	{
		return UpdatePublicKey;
	}

	FString FileSha256(const FString& Path)
	{
#if PLATFORM_WINDOWS
		const TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path, FILEREAD_Silent));
		if (!Reader) { return FString(); }
		FSha256 Hash;
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(4 * 1024 * 1024);
		for (int64 Left = Reader->TotalSize(); Left > 0;)
		{
			const int64 Piece = FMath::Min<int64>(Left, Buffer.Num());
			Reader->Serialize(Buffer.GetData(), Piece);
			if (Reader->IsError()) { return FString(); }
			Hash.Add(Buffer.GetData(), Piece);
			Left -= Piece;
		}
		uint8 Digest[32];
		return Hash.Finish(Digest) ? BytesToHexLower(Digest, sizeof(Digest)) : FString();
#else
		return FString();
#endif
	}

	FString CurrentVersion()
	{
		FString Version;
		if (FParse::Value(FCommandLine::Get(), TEXT("F101UpdateVersion="), Version)) { return Version; }
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
		return Version;
	}

	FString RepositoryPage()
	{
		FString Repository = TEXT("corpjorge/101st-floor");
		GConfig->GetString(UpdateSection, TEXT("Repository"), Repository, GGameIni);
		return TEXT("https://github.com/") + Repository;
	}

	FString InstalledFolder()
	{
		FString Folder;
#if PLATFORM_WINDOWS
		// Written by the installer (Resources/Installer/Product.wxs).
		FWindowsPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, TEXT("Software\\101st Floor\\Installer"), TEXT("InstallFolder"), Folder);
#endif
		return Folder;
	}

	bool IsInstalledCopy()
	{
		FString Installed = InstalledFolder();
		if (Installed.IsEmpty()) { return false; }
		FString Running = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
		FPaths::NormalizeDirectoryName(Installed);
		FPaths::NormalizeDirectoryName(Running);
		return Installed.Equals(Running, ESearchCase::IgnoreCase);
	}

	bool CheckOnStartup()
	{
		bool bCheck = true;
		GConfig->GetBool(UpdateSection, TEXT("CheckOnStartup"), bCheck, GGameUserSettingsIni);
		return bCheck;
	}

	void SetCheckOnStartup(bool bCheck)
	{
		GConfig->SetBool(UpdateSection, TEXT("CheckOnStartup"), bCheck, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FUpdater& FUpdater::Get()
	{
		static FUpdater Instance;
		return Instance;
	}

	FUpdater::FUpdater()
	{
		Reset();
	}

	void FUpdater::Reset()
	{
		++Generation;
		State = EState::Idle;
		PageUrl.Empty();
		SetupUrl.Empty();
		Available = FManifest();
		Installer.Empty();
		Error = FText::GetEmpty();
		Received = 0;
		Current = CurrentVersion();
		TrustedKey = UpdatePublicKey;
		bDryRun = FParse::Param(FCommandLine::Get(), TEXT("F101UpdateDryRun"));
		if (!FParse::Value(FCommandLine::Get(), TEXT("F101UpdateFeed="), Feed))
		{
			FString Repository = TEXT("corpjorge/101st-floor");
			GConfig->GetString(UpdateSection, TEXT("Repository"), Repository, GGameIni);
			Feed = FString::Printf(TEXT("https://api.github.com/repos/%s/releases/latest"), *Repository);
		}
	}

	void FUpdater::Fail(const FText& Why)
	{
		State = EState::Failed;
		Error = Why;
		F101_LOG(LogFloor101, Warning, TEXT("F101Update: %s"), *Why.ToString());
	}

	void FUpdater::Fetch(const FString& Address, TFunction<void(bool, TArray<uint8>&&)> Done) const
	{
		if (!IsRemote(Address))
		{
			TArray<uint8> Bytes;
			const bool bRead = FFileHelper::LoadFileToArray(Bytes, *Address, FILEREAD_Silent);
			Done(bRead, MoveTemp(Bytes));
			return;
		}
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Address);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("User-Agent"), TEXT("101st-Floor/") + Current);
		Request->SetHeader(TEXT("Accept"), Address.Contains(TEXT("api.github.com")) ? TEXT("application/vnd.github+json") : TEXT("application/octet-stream"));
		Request->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid() || Response->GetResponseCode() != 200)
			{
				Done(false, TArray<uint8>());
				return;
			}
			TArray<uint8> Bytes = Response->GetContent();
			Done(true, MoveTemp(Bytes));
		});
		Request->ProcessRequest();
	}

	void FUpdater::Check()
	{
		if (State == EState::Checking || State == EState::Downloading) { return; }
		const uint32 Ticket = ++Generation;
		State = EState::Checking;
		Error = FText::GetEmpty();
		F101_LOG(LogFloor101, Display, TEXT("F101Update: checking %s (this version %s)"), *Feed, *Current);
		Fetch(Feed, [this, Ticket](bool bOk, TArray<uint8>&& Json)
		{
			if (Ticket != Generation) { return; }
			if (!bOk)
			{
				Fail(LOCTEXT("NoRelease", "No se pudo consultar si hay una versión nueva. Se volverá a intentar en el próximo arranque."));
				return;
			}
			OnRelease(MoveTemp(Json));
		});
	}

	void FUpdater::OnRelease(TArray<uint8>&& Json)
	{
		const TOptional<FRelease> Release = ParseRelease(Utf8(Json));
		if (!Release)
		{
			Fail(LOCTEXT("BadRelease", "La respuesta sobre la última versión no se pudo leer."));
			return;
		}
		PageUrl = Release->PageUrl;
		const FString* ManifestUrl = Release->Assets.Find(ManifestName);
		const FString* SignatureUrl = Release->Assets.Find(SignatureName);
		if (!ManifestUrl || !SignatureUrl)
		{
			Fail(LOCTEXT("Unsigned", "La última versión publicada no trae su descripción firmada; no se puede instalar desde aquí."));
			return;
		}
		const uint32 Ticket = Generation;
		Fetch(*ManifestUrl, [this, Ticket, SignatureUrl = *SignatureUrl, Assets = Release->Assets](bool bOk, TArray<uint8>&& Manifest)
		{
			if (Ticket != Generation) { return; }
			if (!bOk)
			{
				Fail(LOCTEXT("NoManifest", "No se pudo descargar la descripción de la versión nueva."));
				return;
			}
			Fetch(SignatureUrl, [this, Ticket, Manifest = MoveTemp(Manifest), Assets](bool bSigned, TArray<uint8>&& Signature)
			{
				if (Ticket != Generation) { return; }
				if (!bSigned || !VerifySignature(Manifest, Utf8(Signature), TrustedKey))
				{
					Fail(LOCTEXT("BadSignature", "La versión publicada no tiene una firma válida del proyecto; no se instalará."));
					return;
				}
				FString Why;
				const TOptional<FManifest> Parsed = ParseManifest(Utf8(Manifest), Why);
				if (!Parsed)
				{
					Fail(FText::Format(LOCTEXT("BadManifest", "La descripción de la versión nueva no es válida ({0})."), FText::FromString(Why)));
					return;
				}
				const TOptional<FVersion> Newest = FVersion::Parse(Parsed->Version);
				const TOptional<FVersion> Mine = FVersion::Parse(Current);
				if (Mine && !(*Mine < *Newest))
				{
					State = EState::UpToDate;
					F101_LOG(LogFloor101, Display, TEXT("F101Update: up to date (%s, latest %s)"), *Current, *Parsed->Version);
					return;
				}
				const FString* Setup = Assets.Find(Parsed->SetupName);
				if (!Setup)
				{
					Fail(LOCTEXT("NoSetup", "La versión nueva no trae su instalador."));
					return;
				}
				Available = *Parsed;
				SetupUrl = *Setup;
				State = EState::Available;
				F101_LOG(LogFloor101, Display, TEXT("F101Update: %s available (%lld bytes)"), *Available.Version, Available.SetupSize);
			});
		});
	}

	void FUpdater::Download()
	{
		if (State != EState::Available) { return; }
		const FString Folder = FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("101st Floor"), TEXT("Updates"));
		IFileManager::Get().MakeDirectory(*Folder, true);
		const FString Target = FPaths::Combine(Folder, Available.SetupName);
		const FString Partial = Target + TEXT(".part");
		IFileManager::Get().Delete(*Partial, false, true, true);
		const uint32 Ticket = ++Generation;
		State = EState::Downloading;
		Received = 0;
		F101_LOG(LogFloor101, Display, TEXT("F101Update: downloading %s"), *SetupUrl);
		auto Finish = [this, Ticket, Target, Partial](bool bOk)
		{
			if (Ticket != Generation) { return; }
			const bool bMatches = bOk && IFileManager::Get().FileSize(*Partial) == Available.SetupSize &&
				FileSha256(Partial) == Available.SetupSha256;
			if (!bMatches)
			{
				IFileManager::Get().Delete(*Partial, false, true, true);
				Fail(bOk ? LOCTEXT("Corrupt", "El instalador descargado no coincide con la versión firmada; se ha borrado.")
					: LOCTEXT("DownloadFailed", "No se pudo descargar la versión nueva."));
				return;
			}
			IFileManager::Get().Delete(*Target, false, true, true);
			if (!IFileManager::Get().Move(*Target, *Partial))
			{
				Fail(LOCTEXT("NotMoved", "No se pudo preparar el instalador descargado."));
				return;
			}
			Installer = Target;
			State = EState::Ready;
			F101_LOG(LogFloor101, Display, TEXT("F101Update: %s ready at %s"), *Available.Version, *Installer);
		};
		if (!IsRemote(SetupUrl))
		{
			const bool bCopied = IFileManager::Get().Copy(*Partial, *SetupUrl) == COPY_OK;
			Received = Available.SetupSize;
			Finish(bCopied);
			return;
		}
		FArchive* Writer = IFileManager::Get().CreateFileWriter(*Partial);
		if (!Writer)
		{
			Fail(LOCTEXT("NoTemp", "No se pudo escribir en la carpeta temporal."));
			return;
		}
		const TSharedRef<FArchive> Stream = MakeShareable(Writer);
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(SetupUrl);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("User-Agent"), TEXT("101st-Floor/") + Current);
		Request->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));
		Request->SetResponseBodyReceiveStream(Stream);
		Request->OnRequestProgress64().BindLambda([this, Ticket](FHttpRequestPtr, uint64, uint64 BytesReceived)
		{
			if (Ticket == Generation) { Received = static_cast<int64>(BytesReceived); }
		});
		Request->OnProcessRequestComplete().BindLambda([Finish, Stream](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			Stream->Close(); // before the file is measured and hashed
			Finish(bConnected && Response.IsValid() && Response->GetResponseCode() == 200);
		});
		Request->ProcessRequest();
	}

	bool FUpdater::LaunchInstaller()
	{
		if (State != EState::Ready || Installer.IsEmpty()) { return false; }
		// Checked again right before it runs: it waited in a folder the user can write to.
		if (FileSha256(Installer) != Available.SetupSha256)
		{
			Fail(LOCTEXT("Changed", "El instalador descargado ha cambiado desde que se comprobó; no se ejecutará."));
			return false;
		}
		// After a short pause (this application closes meanwhile) the installer runs with a progress bar and no questions;
		// if it succeeds, the installed application opens again.
		const FString Setup = FPaths::ConvertRelativePathToFull(Installer).Replace(TEXT("/"), TEXT("\\"));
		const FString Folder = InstalledFolder();
		FString Arguments = FString::Printf(TEXT("/c ping -n 3 127.0.0.1 >nul & \"%s\" /passive /norestart"), *Setup);
		if (!Folder.IsEmpty())
		{
			Arguments += FString::Printf(TEXT(" && start \"\" \"%s\""), *FPaths::Combine(Folder, TEXT("Floor101.exe")).Replace(TEXT("/"), TEXT("\\")));
		}
		if (bDryRun)
		{
			F101_LOG(LogFloor101, Display, TEXT("F101Update: dry run, would run cmd.exe %s"), *Arguments);
			return false;
		}
		FProcHandle Process = FPlatformProcess::CreateProc(TEXT("cmd.exe"), *Arguments, true, true, true, nullptr, 0, nullptr, nullptr);
		if (!Process.IsValid())
		{
			Fail(LOCTEXT("NotLaunched", "No se pudo abrir el instalador."));
			return false;
		}
		FPlatformProcess::CloseProc(Process);
		F101_LOG(LogFloor101, Display, TEXT("F101Update: installing %s"), *Available.Version);
		return true;
	}

	void FUpdater::OpenReleasePage() const
	{
		const FString Page = PageUrl.IsEmpty() ? RepositoryPage() + TEXT("/releases/latest") : PageUrl;
		FPlatformProcess::LaunchURL(*Page, nullptr, nullptr);
	}

	TOptional<float> FUpdater::GetProgress() const
	{
		if (State != EState::Downloading || Available.SetupSize <= 0) { return {}; }
		return FMath::Clamp(static_cast<float>(static_cast<double>(Received) / static_cast<double>(Available.SetupSize)), 0.0f, 1.0f);
	}

	FText FUpdater::GetStatus() const
	{
		FNumberFormattingOptions Whole;
		Whole.SetMaximumFractionalDigits(0);
		const FText Megabytes = FText::AsNumber(static_cast<double>(Available.SetupSize) / (1024.0 * 1024.0), &Whole);
		switch (State)
		{
		case EState::Checking: return LOCTEXT("Checking", "Buscando una versión nueva…");
		case EState::UpToDate: return FText::Format(LOCTEXT("UpToDate", "Tienes la última versión ({0})."), FText::FromString(Current));
		case EState::Available: return FText::Format(LOCTEXT("Available", "Hay una versión nueva: {0} ({1} MB)."), FText::FromString(Available.Version), Megabytes);
		case EState::Downloading:
		{
			const TOptional<float> Done = GetProgress();
			return FText::Format(LOCTEXT("Downloading", "Descargando {0}: {1} %"), FText::FromString(Available.Version),
				FText::AsNumber(FMath::RoundToInt(100.0f * Done.Get(0.0f))));
		}
		case EState::Ready: return FText::Format(LOCTEXT("Ready", "{0} descargada y comprobada. Instálala cuando quieras: 101st Floor se cerrará y volverá a abrirse."),
			FText::FromString(Available.Version));
		case EState::Failed: return Error;
		default: return LOCTEXT("Idle", "Todavía no se ha buscado una versión nueva.");
		}
	}

	namespace
	{
		// -F101UpdateCheck (CheckPackage.ps1 -Check update): check, download and verify, then stop before installing.
		void RunAutomatedCheck()
		{
			FUpdater::Get().Check();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Elapsed = 0.0f, bDownloading = false](float Delta) mutable
			{
				Elapsed += Delta;
				FUpdater& Updater = FUpdater::Get();
				auto Finish = [](bool bPass, const TCHAR* Stage)
				{
					F101_LOG(LogFloor101, Display, TEXT("F101UpdateCheck: %s result=%s"), Stage, bPass ? TEXT("PASS") : TEXT("FAIL"));
					FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
					return false;
				};
				if (Elapsed > 900.0f) { return Finish(false, TEXT("timeout")); }
				switch (Updater.GetState())
				{
				case EState::Available:
					if (!bDownloading)
					{
						bDownloading = true;
						Updater.Download();
					}
					return true;
				case EState::Ready:
					(void)Updater.LaunchInstaller(); // with -F101UpdateDryRun it only logs the command
					return Finish(true, TEXT("ready"));
				case EState::UpToDate: return Finish(false, TEXT("up_to_date"));
				case EState::Failed: return Finish(false, TEXT("failed"));
				default: return true;
				}
			}));
		}
	}

	void Startup(UFloor101GameViewportClient*)
	{
		if (GIsEditor || GIsAutomationTesting) { return; }
		const TCHAR* Command = FCommandLine::Get();
		if (FParse::Param(Command, TEXT("F101UpdateCheck")))
		{
			RunAutomatedCheck();
			return;
		}
		FString Capture;
		if (FParse::Param(Command, TEXT("F101PackageCheck")) || FParse::Param(Command, TEXT("F101DisplayCheck")) ||
			FParse::Param(Command, TEXT("F101HDRCheck")) || FParse::Value(Command, TEXT("F101UICapture="), Capture))
		{
			return;
		}
		if (CheckOnStartup())
		{
			FUpdater::Get().Check();
		}
	}
}

#undef LOCTEXT_NAMESPACE
