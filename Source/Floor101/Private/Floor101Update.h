#pragma once

#include "CoreMinimal.h"

class UFloor101GameViewportClient;

// Updates from the project's GitHub releases (ADR-0039). A release carries a manifest, signed with the project's key,
// that names the installer of its version; a newer one is downloaded, checked against the manifest and run in passive
// mode while the application closes. Only GitHub is asked: nothing about the user or the project is sent.
namespace Floor101::Update
{
	// "major.minor.patch": a leading "v" and anything from a '-' or '+' on are ignored.
	struct FVersion
	{
		int32 Major = 0;
		int32 Minor = 0;
		int32 Patch = 0;

		static TOptional<FVersion> Parse(const FString& Text);
		bool operator<(const FVersion& Other) const;
		FString ToString() const;
	};

	// The signed description of a release: the installer that brings its version.
	struct FManifest
	{
		FString Version;
		FString SetupName;     // a plain file name ending in .exe
		int64 SetupSize = 0;
		FString SetupSha256;   // lowercase hex
	};
	// Reads and checks it (product, version, file name, size, hash); unset, with the reason, otherwise.
	TOptional<FManifest> ParseManifest(const FString& Json, FString& OutError);

	// What GitHub says about a release: its web page and the download address of each asset, by name.
	struct FRelease
	{
		FString PageUrl;
		TMap<FString, FString> Assets;
	};
	TOptional<FRelease> ParseRelease(const FString& Json);

	inline const TCHAR* ManifestName = TEXT("101st-Floor-update.json");
	inline const TCHAR* SignatureName = TEXT("101st-Floor-update.json.sig");

	// ECDSA P-256 with SHA-256: the signature r||s and the public key X||Y, both in Base64 (Windows CNG verifies it).
	bool VerifySignature(TConstArrayView<uint8> Bytes, const FString& SignatureBase64, const FString& PublicKeyBase64);
	// The key releases are signed with (Scripts/UpdateSigningKey.ps1).
	const TCHAR* TrustedPublicKey();
	// SHA-256 of a file in lowercase hex; empty if it cannot be read.
	FString FileSha256(const FString& Path);

	// This build's version (-F101UpdateVersion= pretends another one).
	FString CurrentVersion();
	// The project's page on GitHub ([Floor101.Update] Repository in DefaultGame.ini).
	FString RepositoryPage();
	// Where the installer put the application (its record in the user's registry); empty for a portable copy.
	FString InstalledFolder();
	// Whether this copy is the installed one: only that one installs updates itself.
	bool IsInstalledCopy();

	// Local preference: look for a new version when the application starts (on by default).
	bool CheckOnStartup();
	void SetCheckOnStartup(bool bCheck);

	enum class EState : uint8
	{
		Idle,
		Checking,
		UpToDate,
		Available,   // a newer, correctly signed version exists
		Downloading,
		Ready,       // its installer is downloaded and matches the manifest
		Failed,
	};

	class FUpdater
	{
	public:
		static FUpdater& Get();

		// Asks for the latest release (unless a check or a download is under way).
		void Check();
		// Downloads and checks the installer of the available version; Ready when done.
		void Download();
		// Runs the downloaded installer in passive mode, which reopens the application when it ends; the caller has dealt
		// with unsaved work and closes the application when this returns true. -F101UpdateDryRun only logs the command.
		bool LaunchInstaller();
		// The release's page in the browser (a portable copy updates by hand from there).
		void OpenReleasePage() const;

		EState GetState() const { return State; }
		const FManifest& GetAvailable() const { return Available; }
		// What to tell the user about the last check or download.
		FText GetStatus() const;
		// 0..1 while downloading.
		TOptional<float> GetProgress() const;

		// Test seams: where the release is read from (an https URL by default, or a local file whose asset addresses are
		// local files) and what this build is.
		void SetFeed(const FString& Address) { Feed = Address; }
		void SetCurrentVersion(const FString& Version) { Current = Version; }
		void SetTrustedKey(const FString& PublicKeyBase64) { TrustedKey = PublicKeyBase64; }
		void SetDryRun(bool bOnlyLog) { bDryRun = bOnlyLog; }
		void Reset();

	private:
		FUpdater();
		void Fail(const FText& Why);
		void Fetch(const FString& Address, TFunction<void(bool, TArray<uint8>&&)> Done) const;
		void OnRelease(TArray<uint8>&& Json);

		EState State = EState::Idle;
		FString Feed;
		FString Current;
		FString TrustedKey;
		bool bDryRun = false;
		FString PageUrl;
		FString SetupUrl;
		FManifest Available;
		FString Installer; // downloaded and checked
		FText Error;
		int64 Received = 0;
		uint32 Generation = 0; // answers to an older check or download are ignored
	};

	// At startup: the automated check (-F101UpdateCheck), or a quiet check if the preference allows and nothing else
	// (an editor, a test, a package check) runs the application.
	void Startup(UFloor101GameViewportClient* Client);
}
