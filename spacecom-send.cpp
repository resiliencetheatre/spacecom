#include "normApi.h"
#include "spacecom_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef _WIN32
const char DIR_DELIMITER = '\\';
#else
const char DIR_DELIMITER = '/';
#endif

static void Usage()
{
    PrintCommonUsage("spacecom-send", "<file>");
}

int main(int argc, char* argv[])
{
    SessionConfig cfg;
    const char* file_path = nullptr;

    int i = 1;
    while (i < argc)
    {
        if (0 == strcmp("addr", argv[i]))
        {
            if (!ParseSessionArg(i, argc, argv, cfg, "spacecom-send", "<file>"))
                return 1;
        }
        else if (nullptr == file_path)
        {
            file_path = argv[i++];
        }
        else
        {
            fprintf(stderr, "spacecom-send error: unexpected argument \"%s\"\n", argv[i]);
            Usage();
            return 1;
        }
    }

    if (nullptr == file_path)
    {
        fprintf(stderr, "spacecom-send error: missing file path\n");
        Usage();
        return 1;
    }

    const char* file_name = strrchr(file_path, DIR_DELIMITER);
    file_name = file_name ? (file_name + 1) : file_path;

    const bool is_multicast = IsMulticastAddress(cfg.session_addr);

    NormInstanceHandle instance = NormCreateInstance();
    if (NORM_INSTANCE_INVALID == instance)
    {
        fprintf(stderr, "spacecom-send error: NormCreateInstance() failed\n");
        return 2;
    }

    NormSessionHandle session = NormCreateSession(instance,
                                                  cfg.session_addr.c_str(),
                                                  cfg.session_port,
                                                  NORM_NODE_ANY);
    if (NORM_SESSION_INVALID == session)
    {
        fprintf(stderr, "spacecom-send error: NormCreateSession() failed\n");
        NormDestroyInstance(instance);
        return 2;
    }

    NormSetDebugLevel(3);

    struct timeval now;
    gettimeofday(&now, nullptr);
    srand((unsigned int)now.tv_sec);

    NormSetTxRate(session, 2.0e+06);

    if (!is_multicast)
    {
        NormSetTxPort(session, cfg.session_port + 1, true);
        NormSetTxOnly(session, true, true);
    }

    NormSessionId session_id = (NormSessionId)rand();
    fprintf(stderr, "starting NORM file sender for %s/%hu ...\n",
            cfg.session_addr.c_str(), cfg.session_port);

    if (!NormStartSender(session, session_id, 1024 * 1024, 1000, 64, 16))
    {
        fprintf(stderr, "spacecom-send error: NormStartSender() failed\n");
        NormDestroySession(session);
        NormDestroyInstance(instance);
        return 2;
    }

    NormObjectHandle tx_file =
        NormFileEnqueue(session, file_path, file_name, (UINT16)strlen(file_name));
    if (NORM_OBJECT_INVALID == tx_file)
    {
        fprintf(stderr, "spacecom-send error: NormFileEnqueue() failed for \"%s\"\n", file_path);
        NormStopSender(session);
        NormDestroySession(session);
        NormDestroyInstance(instance);
        return 2;
    }

    bool keep_going = true;
    while (keep_going)
    {
        NormEvent ev;
        if (!NormGetNextEvent(instance, &ev))
            continue;

        switch (ev.type)
        {
            case NORM_TX_QUEUE_VACANCY:
                break;
            case NORM_TX_QUEUE_EMPTY:
                fprintf(stderr, "spacecom-send: NORM_TX_QUEUE_EMPTY event...\n");
                break;
            case NORM_TX_OBJECT_PURGED:
                fprintf(stderr, "spacecom-send: NORM_TX_OBJECT_PURGED event ...\n");
                break;
            case NORM_TX_FLUSH_COMPLETED:
                fprintf(stderr, "spacecom-send: NORM_TX_FLUSH_COMPLETED event ...\n");
                keep_going = false;
                break;
            default:
                break;
        }
    }

    NormStopSender(session);
    NormDestroySession(session);
    NormDestroyInstance(instance);

    fprintf(stderr, "spacecom-send: Done.\n");
    return 0;
}
