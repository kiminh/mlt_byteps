#include "app_context.h"

int AppContext::ParseArgument(int argc, char *argv[]) {
  int err = 0;
  static struct option long_options[] = {
      {"buffer-size", required_argument, 0, 'b'},  // NOLINT(*)
      {"message-size", required_argument, 0, 'm'}, // NOLINT(*)
      {"tos", required_argument, 0, 'T'},          // NOLINT(*)
      {"connections", required_argument, 0, 'C'},  // NOLINT(*)
      {"recv-threads", required_argument, 0, 'R'}, // NOLINT(*)
      {"send-threads", required_argument, 0, 'S'}, // NOLINT(*)
      {0, 0, 0, 0}                                 // NOLINT(*)
  };
  while (1) {
    int option_index = 0, c;
    c = getopt_long(argc, argv, "b:m:T:C:R:S:", long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 'b': {
      int buffer_size = atoi(optarg);
      conf_.set_buffer_size(buffer_size);
      break;
    }
    case 'm': {
      int message_size = atoi(optarg);
      conf_.set_message_size(message_size);
      break;
    }
    case 'T': {
      int tos = atoi(optarg);
      conf_.set_tos(tos);
      break;
    }
    case 'C': {
      int num_connections = atoi(optarg);
      conf_.set_num_conn(num_connections);
      break;
    }
    case 'R': {
      int recv_threads = atoi(optarg);
      conf_.set_recv_threads(recv_threads);
      break;
    }
    case 'S': {
      int send_threads = atoi(optarg);
      conf_.set_send_threads(send_threads);
      break;
    }
    case '?':
    default:
      err = 1;
      goto out;
    }
  }
  if (optind < argc) {
    if (optind + 1 != argc) {
      err = 1;
      goto out;
    }
    std::string host = std::string(argv[optind]);
    conf_.set_is_server(false);
    conf_.set_host(host);
  }
out:
  return err;
}

void AppContext::ShowUsage(const char *app) {
  fprintf(stdout, "Usage:\n");
  fprintf(stdout, "  %s           start a server and wait for connection\n", app);
  fprintf(stdout, "  %s <host>    connect to server at <host>\n", app);
  fprintf(stdout, "\nOptions:\n");
  fprintf(stdout, "  -m, --message-size=<size>  message size\n");
  fprintf(stdout, "  -b, --buffer-size=<size>   receiver buffer size for server\n");
  fprintf(stdout, "  -T, --tos=<uint8>          IP ToS\n");
  fprintf(stdout, "  -C, --connections=<int>    number of concurrent connections\n");
  fprintf(stdout, "  -R, --recv-threads=<int>   number of threads for receiving\n");
  fprintf(stdout, "  -S, --send-threads=<int>   number of threads for sending\n");
}
