#pragma once

struct players_api
{
	virtual size_t GetMaxClients() = 0;
	virtual bool Init() = 0;
	virtual client_t* GetClient(size_t index) = 0;
	virtual model_t* GetModel(size_t model_index) = 0;	
	virtual void SendPacket(unsigned int length, void* data, const netchan_t& chan) = 0;
	virtual double GetRealTime() = 0;


	inline static void NetadrToSockadr(const netadr_t* a, struct sockaddr* s)
	{
		memset(s, 0, sizeof(*s));

		auto s_in = (sockaddr_in*)s;

		switch (a->type)
		{
		case NA_BROADCAST:
			s_in->sin_family = AF_INET;
			s_in->sin_addr.s_addr = INADDR_BROADCAST;
			s_in->sin_port = a->port;
			break;
		case NA_IP:
			s_in->sin_family = AF_INET;
			s_in->sin_addr.s_addr = *(int*)&a->ip;
			s_in->sin_port = a->port;
			break;
#ifdef _WIN32
		case NA_IPX:
			s->sa_family = AF_IPX;
			memcpy(s->sa_data, a->ipx, 10);
			*(unsigned short*)&s->sa_data[10] = a->port;
			break;
		case NA_BROADCAST_IPX:
			s->sa_family = AF_IPX;
			memset(&s->sa_data, 0, 4);
			memset(&s->sa_data[4], 255, 6);
			*(unsigned short*)&s->sa_data[10] = a->port;
			break;
#endif // _WIN32
		default:
			break;
		}
	}
};

struct rehlds_api : players_api
{
	bool Init() override
	{
		// todo check for linux hw.so (LAN)
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
	void SendPacket(unsigned int length, void* data, const netchan_t& chan)
	{
		g_RehldsFuncs->NET_SendPacket(length, data, chan.remote_address);
	}
};


struct hlds_api : players_api
{
	bool Init() override
	{
		CDynPatcher patcher;

#ifdef _WIN32
		if (GetModuleHandleA("hw.dll"))
		{
			patcher.Init("hw.dll");
		}
		else
		{
			patcher.Init("swds.dll");

		}
		
		svs = decltype(svs)(patcher.FindPatternIDA("A1 *? ? ? ? 83 C4 ? 85 C0 74 ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? C3 8B 0D ? ? ? ?"));
		sv = decltype(sv)(patcher.FindPatternIDA("A1 *? ? ? ? 85 C0 74 ? DD ? ? ? ? ? A1 ? ? ? ?"));
		ip_sockets = decltype(ip_sockets)(patcher.FindPatternIDA("8B 34 BD *? ? ? ? 85 F6 0F 84 46 01 00 00"));
		real_time =
			(double*)(patcher.FindPatternIDA("DD ? *? ? ? ? 8B 45 08 D9 ?"));
		

#elif __linux__
		patcher.Init("engine_i486.so");

		svs = decltype(svs)(patcher.FindPatternIDA("83 3D *? ? ? ? ? 0F 9F C0 89 04 24 E8 ? ? ? ? A1 ? ? ? ?"));
		sv = decltype(sv)(patcher.FindPatternIDA("A1 *? ? ? ? 85 C0 75 ? 83 C4 ? 5B 5E"));

		ip_sockets = decltype(ip_sockets)(patcher.FindPatternIDA("8B 1C AD *? ? ? ? 83 FB FF 0F 84 06 01 00 00"));
		if (!ip_sockets)
		{

		}
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
	void SendPacket(unsigned int length, void* data, const netchan_t& chan)
	{
		auto sock = ip_sockets[chan.sock];
		struct sockaddr addr;
		NetadrToSockadr(&chan.remote_address, &addr);
		sendto((SOCKET)sock, static_cast<const char*>(data), length, 0, &addr, sizeof(addr));
	}
	double GetRealTime()
	{
		if (real_time)
			return *real_time;

		else return 0.0;
	}

	inline static server_static_t *svs;
	inline static server_t* sv;
	inline static double* real_time = NULL;
	inline static SOCKET** ip_sockets = nullptr;
};
extern std::unique_ptr<players_api> api;