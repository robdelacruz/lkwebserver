// setopt pattern sample:

void lk_httpserver_setopt(LKHttpServer *server, LKHttpServerOpt opt, ...) {
    char *homedir;
    char *port;
    char *host;
    char *cgidir;
    char *k, *v;
    LKHttpServerSettings *settings = server->settings;

    va_list args;
    va_start(args, opt);

    switch(opt) {
    case LKHTTPSERVEROPT_HOMEDIR:
        homedir = va_arg(args, char*);
        lk_string_assign(settings->homedir, homedir);
        lk_string_chop_end(settings->homedir, "/");
        break;
    case LKHTTPSERVEROPT_PORT:
        port = va_arg(args, char*);
        lk_string_assign(settings->port, port);
        break;
    case LKHTTPSERVEROPT_HOST:
        host = va_arg(args, char*);
        lk_string_assign(settings->host, host);
        break;
    case LKHTTPSERVEROPT_CGIDIR:
        cgidir = va_arg(args, char*);
        if (strlen(cgidir) == 0) {
            break;
        }
        lk_string_assign(settings->cgidir, cgidir);

        // Surround cgi dir with slashes: /cgi-bin/ for easy uri matching.
        if (!lk_string_starts_with(settings->cgidir, "/")) {
            lk_string_prepend(settings->cgidir, "/");
        }
        if (!lk_string_ends_with(settings->cgidir, "/")) {
            lk_string_append(settings->cgidir, "/");
        }
        break;
    case LKHTTPSERVEROPT_ALIAS:
        k = va_arg(args, char*);
        v = va_arg(args, char*);
        lk_stringtable_set(settings->aliases, k, v);
        break;
    case LKHTTPSERVEROPT_PROXYPASS:
        k = va_arg(args, char*);
        v = va_arg(args, char*);
        lk_stringtable_set(settings->proxypass, k, v);
        break;
    default:
        break;
    }

    va_end(args);
}


