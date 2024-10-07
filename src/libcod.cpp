#include "libcod.hpp"

#include "cracking.hpp"
#include "shared.hpp"

// Stock cvars
cvar_t *fs_game;
cvar_t *sv_maxclients;
cvar_t *sv_maxPing;
cvar_t *sv_minPing;
cvar_t *sv_privateClients;
cvar_t *sv_privatePassword;
cvar_t *sv_reconnectlimit;

// Custom cvars
cvar_t *fs_callbacks;
cvar_t *fs_callbacks_additional;
cvar_t *sv_botHook;
cvar_t *sv_cracked;

// Game lib objects
gentity_t *g_entities;

// Game lib functions
ClientCommand_t ClientCommand;
Scr_IsSystemActive_t Scr_IsSystemActive;
Scr_GetNumParam_t Scr_GetNumParam;
Scr_GetInt_t Scr_GetInt;
Scr_GetString_t Scr_GetString;
Scr_AddBool_t Scr_AddBool;
Scr_AddInt_t Scr_AddInt;
Scr_AddFloat_t Scr_AddFloat;
Scr_AddString_t Scr_AddString;
Scr_AddUndefined_t Scr_AddUndefined;
Scr_AddVector_t Scr_AddVector;
Scr_MakeArray_t Scr_MakeArray;
Scr_AddArray_t Scr_AddArray;
Scr_GetFunction_t Scr_GetFunction;
Scr_GetMethod_t Scr_GetMethod;
Scr_Error_t Scr_Error;
Scr_LoadScript_t Scr_LoadScript;
Scr_GetFunctionHandle_t Scr_GetFunctionHandle;
Scr_ExecEntThread_t Scr_ExecEntThread;
Scr_FreeThread_t Scr_FreeThread;
trap_Argv_t trap_Argv;
va_t va;

// Stock callbacks
int codecallback_startgametype = 0;
int codecallback_playerconnect = 0;
int codecallback_playerdisconnect = 0;
int codecallback_playerdamage = 0;
int codecallback_playerkilled = 0;

// Custom callbacks
int codecallback_playercommand = 0;

callback_t callbacks[] =
{
    {&codecallback_startgametype, "CodeCallback_StartGameType", false},
    {&codecallback_playerconnect, "CodeCallback_PlayerConnect", false},
    {&codecallback_playerdisconnect, "CodeCallback_PlayerDisconnect", false},
    {&codecallback_playerdamage, "CodeCallback_PlayerDamage", false},
    {&codecallback_playerkilled, "CodeCallback_PlayerKilled", false},

    {&codecallback_playercommand, "CodeCallback_PlayerCommand", true}
};

customPlayerState_t customPlayerState[MAX_CLIENTS];

cHook *hook_Com_Init;
cHook *hook_GScr_LoadGameTypeScript;
cHook *hook_SV_BotUserMove;
cHook *hook_Sys_LoadDll;

void custom_Com_Init(char *commandLine)
{
    hook_Com_Init->unhook();
    void (*Com_Init)(char *commandLine);
    *(int*)&Com_Init = hook_Com_Init->from;
    Com_Init(commandLine);
    hook_Com_Init->hook();
    
    // Get references to stock cvars
    fs_game = Cvar_FindVar("fs_game");
    sv_maxclients = Cvar_FindVar("sv_maxclients");
    sv_maxPing = Cvar_FindVar("sv_maxPing");
    sv_minPing = Cvar_FindVar("sv_minPing");
    sv_privateClients = Cvar_FindVar("sv_privateClients");
    sv_privatePassword = Cvar_FindVar("sv_privatePassword");
    sv_reconnectlimit = Cvar_FindVar("sv_reconnectlimit");

    // Register custom cvars
    Cvar_Get("libcod", "1", CVAR_SERVERINFO);

    fs_callbacks = Cvar_Get("fs_callbacks", "maps/mp/gametypes/_callbacksetup", CVAR_ARCHIVE);
    fs_callbacks_additional = Cvar_Get("fs_callbacks_additional", "", CVAR_ARCHIVE);
    sv_botHook = Cvar_Get("sv_botHook", "0", CVAR_ARCHIVE);
    sv_cracked = Cvar_Get("sv_cracked", "0", CVAR_ARCHIVE);
}

const char* hook_AuthorizeState(int arg)
{
    const char* s = Cmd_Argv(arg);
    if(sv_cracked->integer && !strcmp(s, "deny"))
        return "accept";
    return s;
}

void custom_SV_BotUserMove(client_t *client)
{
    if (!sv_botHook->integer)
    {
        hook_SV_BotUserMove->unhook();
        void (*SV_BotUserMove)(client_t *client);
        *(int*)&SV_BotUserMove = hook_SV_BotUserMove->from;
        SV_BotUserMove(client);
        hook_SV_BotUserMove->hook();
        return;
    }
    
    int num;
    usercmd_t ucmd = {0};

    if(client->gentity == NULL)
        return;

    num = client - svs.clients;
    ucmd.serverTime = svs.time;

    playerState_t *ps = SV_GameClientNum(num);
    gentity_t *ent = &g_entities[num];

    ucmd.weapon = (byte)(ps->weapon & 0xFF);

    if(ent->client == NULL)
        return;

    if (ent->client->sess.archiveTime == 0)
    {
        ucmd.buttons = customPlayerState[num].botButtons;

        VectorCopy(ent->client->sess.cmd.angles, ucmd.angles);
    }

    client->deltaMessage = client->netchan.outgoingSequence - 1;
    SV_ClientThink(client, &ucmd);
}

void custom_GScr_LoadGameTypeScript()
{
    hook_GScr_LoadGameTypeScript->unhook();
    void (*GScr_LoadGameTypeScript)();
    *(int*)&GScr_LoadGameTypeScript = hook_GScr_LoadGameTypeScript->from;
    GScr_LoadGameTypeScript();
    hook_GScr_LoadGameTypeScript->hook();

    unsigned int i;
    
    if(*fs_callbacks_additional->string)
        if(!Scr_LoadScript(fs_callbacks_additional->string))
            Com_DPrintf("custom_GScr_LoadGameTypeScript: Scr_LoadScript for fs_callbacks_additional failed.\n");

    for (i = 0; i < sizeof(callbacks) / sizeof(callbacks[0]); i++)
    {
        if(callbacks[i].custom)
            *callbacks[i].pos = Scr_GetFunctionHandle(fs_callbacks_additional->string, callbacks[i].name);
        else
            *callbacks[i].pos = Scr_GetFunctionHandle(fs_callbacks->string, callbacks[i].name);

        /*if (*callbacks[i].pos && g_debugCallbacks->integer)
            Com_Printf("%s found @ %p\n", callbacks[i].name, scrVarPub.programBuffer + *callbacks[i].pos);*/ //TODO: verify scrVarPub_t
    }
}

const char *getShortVersionFromProtocol(int protocol)
{
    switch (protocol)
    {
        case 1: return "1.1";
        case 6: return "1.5";
        default: return "unknown";
    }
}
void custom_SV_DirectConnect(netadr_t from)
{
    char userinfo[MAX_INFO_STRING];
    int i;
    client_t *cl, *newcl;
    gentity_t *ent;
    int clientNum;
    int version;
    int challenge;
    int qport;
    const char *PbAuthAddress;
    const char *PbAuthResult;
    const char *password;
    int startIndex;
    const char *denied;
    int count;
    int guid;
    
    Com_DPrintf("SV_DirectConnect()\n");
    
    I_strncpyz(userinfo, Cmd_Argv(1), sizeof(userinfo));
    version = atoi(Info_ValueForKey(userinfo, "protocol"));
    
    if (version != 1 && version != 6)
    {
        NET_OutOfBandPrint(NS_SERVER, from, va("error\nEXE_SERVER_IS_DIFFERENT_VER\x15%s\n", "1.5"));
        Com_DPrintf("    rejected connect from protocol version %i (should be %i or %i)\n", version, 1, 6);
        return;
    }
    
    challenge = atoi(Info_ValueForKey(userinfo, "challenge"));
    qport = atoi(Info_ValueForKey(userinfo, "qport"));
    
    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
    {
        if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress)
            && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port))
        {
            if ((svs.time - cl->lastConnectTime) < (sv_reconnectlimit->integer * 1000))
            {
                Com_DPrintf("%s:reconnect rejected : too soon\n", NET_AdrToString(from));
                return;
            }
            break;
        }
    }

    guid = 0;

    if (!NET_IsLocalAddress(from))
    {
        int ping;

        for (i = 0 ; i < MAX_CHALLENGES ; i++)
        {
            if (NET_CompareAdr(from, svs.challenges[i].adr))
            {
                if (challenge == svs.challenges[i].challenge)
                {
                    guid = svs.challenges[i].guid;
                    break;
                }
            }
        }

        if (i == MAX_CHALLENGES)
        {
            NET_OutOfBandPrint(NS_SERVER, from, "error\nEXE_BAD_CHALLENGE");
            return;
        }

        if (svs.challenges[i].firstPing == 0)
        {
            ping = svs.time - svs.challenges[i].pingTime;
            svs.challenges[i].firstPing = ping;
        }
        else
        {
            ping = svs.challenges[i].firstPing;
        }

        Com_Printf("Client %i connecting with %i challenge ping from %s\n", i, ping, NET_AdrToString(from));
        svs.challenges[i].connected = qtrue;

        if (!Sys_IsLANAddress(from))
        {
            if (sv_minPing->integer && ping < sv_minPing->integer)
            {
                NET_OutOfBandPrint(NS_SERVER, from, "error\nEXE_ERR_HIGH_PING_ONLY");
                Com_DPrintf("Client %i rejected on a too low ping\n", i);
                return;
            }

            if (sv_maxPing->integer && ping > sv_maxPing->integer)
            {
                NET_OutOfBandPrint(NS_SERVER, from, "error\nEXE_ERR_LOW_PING_ONLY");
                Com_DPrintf("Client %i rejected on a too high ping: %i\n", i, ping);
                return;
            }
        }
    }

    if (!NET_IsLocalAddress(from))
    {
        PbAuthAddress = NET_AdrToString(from);
    }
    else
    {
        PbAuthAddress = "localhost";
    }
    PbAuthResult = PbAuthClient(PbAuthAddress, atoi(Info_ValueForKey(userinfo, "cl_punkbuster")), Info_ValueForKey(userinfo, "cl_guid"));
    if (!PbAuthResult)
    {
        for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
        {
            if (cl->state == CS_FREE)
            {
                continue;
            }
            if (NET_CompareBaseAdr(from, cl->netchan.remoteAddress)
                && (cl->netchan.qport == qport || from.port == cl->netchan.remoteAddress.port))
            {
                Com_Printf("%s:reconnect\n", NET_AdrToString(from));

                if(cl->state > CS_ZOMBIE )
                    SV_FreeClient(cl);

                newcl = cl;
                goto LAB_0808a83b;
            }
        }

        password = Info_ValueForKey(userinfo, "password");
        if (!strcmp(password, sv_privatePassword->string))
        {
            startIndex = 0;
        }
        else
        {
            startIndex = sv_privateClients->integer;
        }

        newcl = NULL;
        for (i = startIndex; i < sv_maxclients->integer; i++)
        {
            cl = &svs.clients[i];
            if (cl->state == CS_FREE)
            {
                newcl = cl;
                break;
            }
        }
        if (!newcl)
        {
            NET_OutOfBandPrint(NS_SERVER, from, "error\nEXE_SERVERISFULL");
            Com_DPrintf("Rejected a connection.\n");
            return;
        }
        
        cl->reliableAcknowledge = 0;
        cl->reliableSequence = 0;
LAB_0808a83b:
        memset(newcl, 0, sizeof(client_t));
        clientNum = newcl - svs.clients;
        ent = SV_GentityNum(clientNum);
        newcl->gentity = ent;
        newcl->clscriptid = Scr_AllocArray();
        newcl->challenge = challenge;

        // Save client protocol version
        customPlayerState[clientNum].protocolVersion = version;
        Com_Printf("Connecting player #%i runs on version %s\n", clientNum, getShortVersionFromProtocol(version));

        newcl->guid = guid;

        Netchan_Setup(NS_SERVER, &newcl->netchan, from, qport);
        I_strncpyz(newcl->userinfo, userinfo, sizeof(newcl->userinfo));

        denied = (char *)VM_Call(gvm, 4, clientNum, newcl->clscriptid);
        if (!denied)
        {
            SV_UserinfoChanged(newcl);
            svs.challenges[i].firstPing = 0;
            NET_OutOfBandPrint(NS_SERVER, from, "connectResponse");
            Com_Printf("Going from CS_FREE to CS_CONNECTED for %s (num %i guid %i)\n", newcl->name, clientNum, newcl->guid);

            newcl->state = CS_CONNECTED;
            newcl->nextSnapshotTime = svs.time;
            newcl->lastPacketTime = svs.time;
            newcl->lastConnectTime = svs.time;
            newcl->gamestateMessageNum = -1;
            
            count = 0;
            for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
            {
                if (svs.clients[i].state >= CS_CONNECTED)
                {
                    count++;
                }
            }
            if (count == 1 || count == sv_maxclients->integer)
            {
                SV_Heartbeat_f();
            }
        }
        else
        {
            NET_OutOfBandPrint(NS_SERVER, from, va("error\n%s", denied));
            Com_DPrintf("Game rejected a connection: %s.\n", denied);
            SV_FreeClientScriptId(newcl);
            return;
        }
    }
    else
    {
        if (!strncasecmp(PbAuthResult, "error\n", 6))
        {
            NET_OutOfBandPrint(NS_SERVER, from, PbAuthResult);
        }
    }
}

void hook_ClientCommand(int clientNum)
{
    if(!Scr_IsSystemActive())
        return;

    if (!codecallback_playercommand)
    {
        ClientCommand(clientNum);
        return;
    }

    stackPushArray();
    int args = Cmd_Argc();
    for (int i = 0; i < args; i++)
    {
        char tmp[MAX_STRINGLENGTH];
        trap_Argv(i, tmp, sizeof(tmp));
        if (i == 1 && tmp[0] >= 20 && tmp[0] <= 22)
        {
            char *part = strtok(tmp + 1, " ");
            while (part != NULL)
            {
                stackPushString(part);
                stackPushArrayLast();
                part = strtok(NULL, " ");
            }
        }
        else
        {
            stackPushString(tmp);
            stackPushArrayLast();
        }
    }
    
    short ret = Scr_ExecEntThread(&g_entities[clientNum], codecallback_playercommand, 1);
    Scr_FreeThread(ret);
}

void ServerCrash(int sig)
{
    int fd;
    FILE *fp;
    void *array[20];
    size_t size = backtrace(array, 20);

    // Write to crash log
    fp = fopen("./crash.log", "a");
    if (fp)
    {
        fd = fileno(fp);
        fseek(fp, 0, SEEK_END);
        fprintf(fp, "Error: Server crashed with signal 0x%x {%d}\n", sig, sig);
        fflush(fp);
        backtrace_symbols_fd(array, size, fd);
    }
    
    // Write to stderr
    fprintf(stderr, "Error: Server crashed with signal 0x%x {%d}\n", sig, sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    system("stty sane");
    exit(1);
}

void *custom_Sys_LoadDll(const char *name, char *fqpath, int (**entryPoint)(int, ...), int (*systemcalls)(int, ...))
{
    hook_Sys_LoadDll->unhook();
    void *(*Sys_LoadDll)(const char *name, char *fqpath, int (**entryPoint)(int, ...), int (*systemcalls)(int, ...));
    *(int*)&Sys_LoadDll = hook_Sys_LoadDll->from;
    void* libHandle = Sys_LoadDll(name, fqpath, entryPoint, systemcalls);
    hook_Sys_LoadDll->hook();

    //// Unprotect game.mp.i386.so
    // See https://github.com/xtnded/codextended/blob/855df4fb01d20f19091d18d46980b5fdfa95a712/src/librarymodule.c#L55
    char libPath[512];
    char buf[512];
    char flags[4];
    void *low, *high;
    FILE *fp;
    
    if(*fs_game->string)
        sprintf(libPath, "%s/game.mp.i386.so", fs_game->string);
    else
        sprintf(libPath, "main/game.mp.i386.so");
    
    fp = fopen("/proc/self/maps", "r");
    if(!fp)
        return 0;

    while (fgets(buf, sizeof(buf), fp))
    {
        if(!strstr(buf, libPath))
            continue;
        if(sscanf (buf, "%p-%p %4c", &low, &high, flags) != 3)
            continue;
        mprotect((void *)low, (int)high-(int)low, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
    fclose(fp);
    ////

    //// Objects
    g_entities = (gentity_t*)dlsym(libHandle, "g_entities");
    ////

    //// Functions
    ClientCommand = (ClientCommand_t)dlsym(libHandle, "ClientCommand");
    Scr_IsSystemActive = (Scr_IsSystemActive_t)dlsym(libHandle, "Scr_IsSystemActive");
    Scr_GetFunctionHandle = (Scr_GetFunctionHandle_t)dlsym(libHandle, "Scr_GetFunctionHandle");
    Scr_GetNumParam = (Scr_GetNumParam_t)dlsym(libHandle, "Scr_GetNumParam");
    Scr_GetFunction = (Scr_GetFunction_t)dlsym(libHandle, "Scr_GetFunction");
    Scr_GetMethod = (Scr_GetMethod_t)dlsym(libHandle, "Scr_GetMethod");
    Scr_Error = (Scr_Error_t)dlsym(libHandle, "Scr_Error");
    Scr_GetInt = (Scr_GetInt_t)dlsym(libHandle, "Scr_GetInt");
    Scr_GetString = (Scr_GetString_t)dlsym(libHandle, "Scr_GetString");
    Scr_AddBool = (Scr_AddBool_t)dlsym(libHandle, "Scr_AddBool");
    Scr_AddInt = (Scr_AddInt_t)dlsym(libHandle, "Scr_AddInt");
    Scr_AddFloat = (Scr_AddFloat_t)dlsym(libHandle, "Scr_AddFloat");
    Scr_AddString = (Scr_AddString_t)dlsym(libHandle, "Scr_AddString");
    Scr_AddUndefined = (Scr_AddUndefined_t)dlsym(libHandle, "Scr_AddUndefined");
    Scr_AddVector = (Scr_AddVector_t)dlsym(libHandle, "Scr_AddVector");
    Scr_MakeArray = (Scr_MakeArray_t)dlsym(libHandle, "Scr_MakeArray");
    Scr_AddArray = (Scr_AddArray_t)dlsym(libHandle, "Scr_AddArray");
    Scr_LoadScript = (Scr_LoadScript_t)dlsym(libHandle, "Scr_LoadScript");
    Scr_ExecEntThread = (Scr_ExecEntThread_t)dlsym(libHandle, "Scr_ExecEntThread");
    Scr_FreeThread = (Scr_FreeThread_t)dlsym(libHandle, "Scr_FreeThread");
    trap_Argv = (trap_Argv_t)dlsym(libHandle, "trap_Argv");
    va = (va_t)dlsym(libHandle, "va");
    ////

    hook_call((int)dlsym(libHandle, "vmMain") + 0xF0, (int)hook_ClientCommand);

    hook_GScr_LoadGameTypeScript = new cHook((int)dlsym(libHandle, "GScr_LoadGameTypeScript"), (int)custom_GScr_LoadGameTypeScript);
    hook_GScr_LoadGameTypeScript->hook();

    return libHandle;
}

class libcod
{
    public:
    libcod()
    {
        printf("------------- libcod -------------\n");
        printf("Compiled on %s %s using g++ %s\n", __DATE__, __TIME__, __VERSION__);

        // Don't inherit lib of parent
        unsetenv("LD_PRELOAD");

        // Crash handlers for debugging
        signal(SIGSEGV, ServerCrash);
        signal(SIGABRT, ServerCrash);
        
        // Otherwise the printf()'s are printed at crash/end on older os/compiler versions
        // See https://github.com/M-itch/libcod/blob/e58d6a01b11c911fbf886659b6ea67795776cf4a/libcod.cpp#L1346
        setbuf(stdout, NULL);

        // Allow to write in executable memory
        mprotect((void *)0x08048000, 0x135000, PROT_READ | PROT_WRITE | PROT_EXEC);
        
        hook_call(0x080894c5, (int)hook_AuthorizeState);
        hook_call(0x0809d8f5, (int)Scr_GetCustomFunction);
        hook_call(0x0809db31, (int)Scr_GetCustomMethod);

        hook_jmp(0x08089e7e, (int)custom_SV_DirectConnect);
        
        hook_Sys_LoadDll = new cHook(0x080d3cdd, (int)custom_Sys_LoadDll);
        hook_Sys_LoadDll->hook();
        hook_Com_Init = new cHook(0x08070ef8, (int)custom_Com_Init);
        hook_Com_Init->hook();
        hook_SV_BotUserMove = new cHook(0x080940d2, (int)custom_SV_BotUserMove);
        hook_SV_BotUserMove->hook();

        printf("Loading complete\n");
        printf("-----------------------------------\n");
    }

    ~libcod()
    {
        printf("Libcod unloaded\n");
        system("stty sane");
    }
};

libcod *lc;
void __attribute__ ((constructor)) lib_load(void)
{
    lc = new libcod;
}
void __attribute__ ((destructor)) lib_unload(void)
{
    delete lc;
}