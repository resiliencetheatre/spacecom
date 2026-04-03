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
    fprintf(stderr,
            "Usage: spacecom-send [addr <addr>/<port>] [acknode <nodeId>] <file>\n");
}

int main(int argc, char* argv[])
{
    SessionConfig cfg;
    const char* file_path = nullptr;
    NormNodeId ack_node_id = 2;  // matches current spacecom-recv local node id

    int i = 1;
    while (i < argc)
    {
        if (0 == strcmp("addr", argv[i]))
        {
            if (!ParseSessionArg(i, argc, argv, cfg, "spacecom-send",
                                 "[addr <addr>/<port>] [acknode <nodeId>] <file>"))
                return 1;
        }
        else if (0 == strcmp("acknode", argv[i]))
        {
            ++i;
            if (i >= argc)
            {
                fprintf(stderr, "spacecom-send error: missing acknode value\n");
                Usage();
                return 1;
            }
            ack_node_id = (NormNodeId)strtoul(argv[i++], nullptr, 10);
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

    // For small receiver groups / unicast, watermark + explicit ACK is a better
    // completion signal than waiting on the default end-of-transmission flush.
    NormSetTxRobustFactor(session, 5);

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

    bool watermark_set = false;
    if (!is_multicast)
    {
        if (!NormAddAckingNode(session, ack_node_id))
        {
            fprintf(stderr, "spacecom-send error: NormAddAckingNode(%lu) failed\n",
                    (unsigned long)ack_node_id);
            NormStopSender(session);
            NormDestroySession(session);
            NormDestroyInstance(instance);
            return 2;
        }

        if (!NormSetWatermark(session, tx_file, true))
        {
            fprintf(stderr, "spacecom-send error: NormSetWatermark() failed\n");
            NormStopSender(session);
            NormDestroySession(session);
            NormDestroyInstance(instance);
            return 2;
        }

        watermark_set = true;
        fprintf(stderr, "spacecom-send: waiting for watermark ACK from node %lu ...\n",
                (unsigned long)ack_node_id);
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

            case NORM_TX_OBJECT_SENT:
                fprintf(stderr, "spacecom-send: NORM_TX_OBJECT_SENT event ...\n");
                break;

            case NORM_TX_OBJECT_PURGED:
                fprintf(stderr, "spacecom-send: NORM_TX_OBJECT_PURGED event ...\n");
                break;

            case NORM_TX_WATERMARK_COMPLETED:
                fprintf(stderr, "spacecom-send: NORM_TX_WATERMARK_COMPLETED event ...\n");
                if (watermark_set)
                {
                    NormAckingStatus status = NormGetAckingStatus(session, ack_node_id);
                    fprintf(stderr, "spacecom-send: ack status for node %lu = %d\n",
                            (unsigned long)ack_node_id, (int)status);
                }
                keep_going = false;
                break;

            case NORM_TX_FLUSH_COMPLETED:
                fprintf(stderr, "spacecom-send: NORM_TX_FLUSH_COMPLETED event ...\n");
                if (!watermark_set)
                    keep_going = false;
                break;

            default:
                break;
        }
    }

    if (watermark_set)
        NormRemoveAckingNode(session, ack_node_id);

    NormStopSender(session);
    NormDestroySession(session);
    NormDestroyInstance(instance);

    fprintf(stderr, "spacecom-send: Done.\n");
    return 0;
}
