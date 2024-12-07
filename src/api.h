#pragma once

struct players_api
{
	virtual size_t GetMaxClients() = 0;
	virtual bool Init() = 0;
	virtual client_t* GetClient(size_t index) = 0;
	virtual model_t* GetModel(size_t model_index) = 0;
	virtual void SendPacket(unsigned int length, void *data, const netchan_t &chan) = 0;
	virtual double GetRealTime() = 0;
};

struct rehlds_api : players_api
{
	bool Init() override
	{
#ifdef _WIN32
		if (GetModuleHandleA("hw.dll"))
		{
			return false;
		}
#endif
		
		return RehldsApi_Init();
	}
	client_t* GetClient(size_t index) override
	{
		return g_RehldsSvs->GetClient_t(index);
	}
	size_t GetMaxClients() override
	{
		return g_RehldsSvs->GetMaxClients();
	}
	model_t* GetModel(size_t model_index)
	{
		return g_RehldsApi->GetServerData()->GetModel(model_index);
	}
	double GetRealTime()
	{
		return g_RehldsFuncs->GetRealTime();
	}
	void SendPacket(unsigned int length, void *data, const netchan_t &chan)
	{
		g_RehldsFuncs->NET_SendPacket(length, data, chan.remote_address);
	}
};

void NetadrToSockadr(const netadr_t *a, struct sockaddr *s);

struct hlds_api : players_api
{
	bool Init() override
	{
#ifdef _WIN32
		if (GetModuleHandleA("hw.dll"))
		{
			svs = decltype(svs)(PatternScan::FindPattern("A1 *? ? ? ? 83 C4 ? 85 C0 74 ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? C3 8B 0D ? ? ? ?", "hw.dll"));
			sv = decltype(sv)(PatternScan::FindPattern("A1 *? ? ? ? 85 C0 74 ? DD ? ? ? ? ? A1 ? ? ? ?", "hw.dll"));
			ip_sockets = decltype(ip_sockets)(PatternScan::FindPattern("8B 34 BD *? ? ? ? 85 F6 0F 84 46 01 00 00", "hw.dll"));
			real_time =
				(double*)(PatternScan::FindPattern("DD ? *? ? ? ? 8B 45 08 D9 ?", "hw.dll"));
		}
		else
		{
			svs = decltype(svs)(PatternScan::FindPattern("A1 *? ? ? ? 83 C4 ? 85 C0 74 ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? C3 8B 0D ? ? ? ?", "swds.dll"));
			sv = decltype(sv)(PatternScan::FindPattern("A1 *? ? ? ? 85 C0 74 ? DD ? ? ? ? ? A1 ? ? ? ?", "swds.dll"));
			ip_sockets = decltype(ip_sockets)(PatternScan::FindPattern("8B 34 BD *? ? ? ? 85 F6 0F 84 46 01 00 00", "swds.dll"));
			real_time =
				(double*)(PatternScan::FindPattern("DD ? *? ? ? ? 8B 45 08 D9 ?", "swds.dll"));
		}

#elif __linux__
		svs = decltype(svs)(PatternScan::FindPattern("83 3D *? ? ? ? ? 0F 9F C0 89 04 24 E8 ? ? ? ? A1 ? ? ? ?", "engine_i486.so"));
		sv = decltype(sv)(PatternScan::FindPattern("A1 *? ? ? ? 85 C0 75 ? 83 C4 ? 5B 5E", "engine_i486.so"));
#endif
		return svs != nullptr && sv != nullptr;
	};
	client_t* GetClient(size_t index) override
	{
		return &svs->clients[index];
	};
	size_t GetMaxClients() override
	{
		return svs->maxclients;
	};
	model_t* GetModel(size_t model_index)
	{
		return sv->models[model_index];
	}
	void SendPacket(unsigned int length, void *data, const netchan_t &chan)
	{
		auto sock = ip_sockets[chan.sock];
		struct sockaddr addr;
		NetadrToSockadr(&chan.remote_address, &addr);
		sendto((SOCKET)sock, static_cast<const char *>(data), length, 0, &addr, sizeof(addr));
	}
	double GetRealTime()
	{
		return g_RehldsFuncs->GetRealTime();
	}

	inline static server_static_t *svs;
	inline static server_t* sv;
	inline static SOCKET** ip_sockets = NULL;
	inline static double* real_time = NULL;
};
extern std::unique_ptr<players_api> api;