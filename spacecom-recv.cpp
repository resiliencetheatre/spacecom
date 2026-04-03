#include "normApi.h"
#include "spacecom_common.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef _WIN32
const char DIR_DELIMITER = '\\';
#else
const char DIR_DELIMITER = '/';
#endif

static void Usage()
{
    PrintCommonUsage("spacecom-recv", "<rxCachePath>");
}

int main(int argc, char* argv[])
{
    SessionConfig cfg;
    const char* rx_cache_path = nullptr;

    int i = 1;
    while (i < argc)
    {
        if (0 == strcmp("addr", argv[i]))
        {
            if (!ParseSessionArg(i, argc, argv, cfg, "spacecom-recv", "<rxCachePath>"))
                return 1;
        }
        else if (nullptr == rx_cache_path)
        {
            rx_cache_path = argv[i++];
        }
        else
        {
            fprintf(stderr, "spacecom-recv error: unexpected argument \"%s\"\n", argv[i]);
            Usage();
            return 1;
        }
    }

    if (nullptr == rx_cache_path)
    {
        fprintf(stderr, "spacecom-recv error: missing rx cache path\n");
        Usage();
        return 1;
    }

    const bool is_multicast = IsMulticastAddress(cfg.session_addr);

    NormInstanceHandle instance = NormCreateInstance();
    if (NORM_INSTANCE_INVALID == instance)
    {
        fprintf(stderr, "spacecom-recv error: NormCreateInstance() failed\n");
        return 2;
    }

    fprintf(stderr, "joining session at addr/port %s/%hu\n",
            cfg.session_addr.c_str(), cfg.session_port);

    NormSessionHandle session = NormCreateSession(instance,
                                                  cfg.session_addr.c_str(),
                                                  cfg.session_port,
                                                  2);
    if (NORM_SESSION_INVALID == session)
    {
        fprintf(stderr, "spacecom-recv error: NormCreateSession() failed\n");
        NormDestroyInstance(instance);
        return 2;
    }

    NormSetDebugLevel(3);

    struct timeval now;
    gettimeofday(&now, nullptr);
    srand((unsigned int)now.tv_sec);

    if (!NormSetCacheDirectory(instance, rx_cache_path))
    {
        fprintf(stderr, "spacecom-recv error: NormSetCacheDirectory(\"%s\") failed\n", rx_cache_path);
        NormDestroySession(session);
        NormDestroyInstance(instance);
        return 2;
    }

    if (!is_multicast)
    {
        NormSetDefaultUnicastNack(session, true);
        NormSetTxPort(session, cfg.session_port);
        NormSetRxPortReuse(session, true, NULL, cfg.session_addr.c_str(), cfg.session_port + 1);
    }

    NormSetRxCacheLimit(session, 4096);

    if (!NormStartReceiver(session, 1024 * 1024))
    {
        fprintf(stderr, "spacecom-recv error: NormStartReceiver() failed\n");
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
            case NORM_RX_OBJECT_NEW:
                fprintf(stderr, "spacecom-recv: NORM_RX_OBJECT_NEW event ...\n");
                break;

            case NORM_RX_OBJECT_INFO:
                fprintf(stderr, "spacecom-recv: NORM_RX_OBJECT_INFO event ...\n");
                if (NORM_OBJECT_FILE == NormObjectGetType(ev.object))
                {
                    char file_name[PATH_MAX];
                    strncpy(file_name, rx_cache_path, sizeof(file_name) - 1);
                    file_name[sizeof(file_name) - 1] = '\0';

                    int path_len = (int)strlen(file_name);
                    if (path_len <= 0)
                    {
                        fprintf(stderr, "spacecom-recv error: invalid rx cache path\n");
                        keep_going = false;
                        break;
                    }

                    if (DIR_DELIMITER != file_name[path_len - 1])
                    {
                        if (path_len >= (int)sizeof(file_name) - 1)
                        {
                            fprintf(stderr, "spacecom-recv error: rx cache path too long\n");
                            keep_going = false;
                            break;
                        }
                        file_name[path_len++] = DIR_DELIMITER;
                        file_name[path_len] = '\0';
                    }

                    unsigned short name_len = (unsigned short)(sizeof(file_name) - path_len - 1);
                    name_len = NormObjectGetInfo(ev.object, file_name + path_len, name_len);
                    file_name[path_len + name_len] = '\0';

                    for (char* p = file_name + path_len; '\0' != *p; ++p)
                    {
                        if ('/' == *p)
                            *p = DIR_DELIMITER;
                    }

                    if (!NormFileRename(ev.object, file_name))
                        fprintf(stderr, "spacecom-recv: NormFileRename(%s) error\n", file_name);
                }
                break;

            case NORM_RX_OBJECT_UPDATED:
            {
                NormSize object_size = NormObjectGetSize(ev.object);
                NormSize completed = object_size - NormObjectGetBytesPending(ev.object);
                double percent_complete = 100.0 * ((double)completed / (double)object_size);
                fprintf(stderr, "spacecom-recv: completion status %lu/%lu (%3.0lf%%)\n",
                        (unsigned long)completed,
                        (unsigned long)object_size,
                        percent_complete);
                break;
            }

            case NORM_RX_OBJECT_COMPLETED:
                fprintf(stderr, "spacecom-recv: NORM_RX_OBJECT_COMPLETED event ...\n");
                sleep(5);
                keep_going = false;
                break;

            case NORM_RX_OBJECT_ABORTED:
                fprintf(stderr, "spacecom-recv: NORM_RX_OBJECT_ABORTED event ...\n");
                break;

            case NORM_REMOTE_SENDER_NEW:
                fprintf(stderr, "spacecom-recv: NORM_REMOTE_SENDER_NEW event ...\n");
                break;

            case NORM_REMOTE_SENDER_ACTIVE:
                fprintf(stderr, "spacecom-recv: NORM_REMOTE_SENDER_ACTIVE event ...\n");
                break;

            case NORM_REMOTE_SENDER_INACTIVE:
                fprintf(stderr, "spacecom-recv: NORM_REMOTE_SENDER_INACTIVE event ...\n");
                break;

            default:
                break;
        }
    }

    NormStopReceiver(session);
    NormDestroySession(session);
    NormDestroyInstance(instance);

    fprintf(stderr, "spacecom-recv: Done.\n");
    return 0;
}
