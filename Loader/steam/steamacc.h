#pragma once

#include <string>
#include <cstdint>
#include <d3d11.h>

class SteamAccount
{
public:
	SteamAccount() = default;

	uint64_t GetSteamID64() const { return m_steamID64; }
	const std::string& GetAccountName() const { return m_accountName; }
	const std::string& GetPersonaName() const { return m_personaName; }
	bool IsMostRecent() const { return m_mostRecent; }
	uint64_t GetLastLoginTimestamp() const { return m_timestamp; }

	void SetSteamID64(uint64_t id) { m_steamID64 = id; }
	void SetAccountName(const std::string& name) { m_accountName = name; }
	void SetPersonaName(const std::string& name) { m_personaName = name; }
	void SetRememberPassword(bool v) { m_rememberPassword = v; }
	void SetMostRecent(bool v) { m_mostRecent = v; }
	void SetTimestamp(uint64_t t) { m_timestamp = t; }

	bool CanAutoLogin() const { return m_rememberPassword; }

	ID3D11ShaderResourceView* GetAvatar() const { return m_avatar; }
	void SetAvatar(ID3D11ShaderResourceView* srv)
	{
		if (m_avatar) m_avatar->Release();
		m_avatar = srv;
	}

	enum class AvatarState { NotLoaded, Loading, Loaded, Failed };
	AvatarState GetAvatarState() const { return m_avatarState; }
	void SetAvatarState(AvatarState state) { m_avatarState = state; }

	~SteamAccount()
	{
		if (m_avatar) m_avatar->Release();
	}

private:
	uint64_t m_steamID64 = 0;
	std::string m_accountName;
	std::string m_personaName;
	bool m_rememberPassword = false;
	bool m_mostRecent = false;
	uint64_t m_timestamp = 0;

	ID3D11ShaderResourceView* m_avatar = nullptr;
	AvatarState m_avatarState = AvatarState::NotLoaded;
};