module Server = Server;
module Req = Req;
module Res = Res;
module Method = Method;
module Route = Route;
module Router = Router;
module Query = Query;
module MimeTypes = MimeTypes;
module Middleware = Middleware;
module RequestHandler = RequestHandler;
module Session = Session;
module SessionManager = SessionManager;
module Cookie = Cookie;
module ServerConfig = ServerConfig;
module SessionConfig = SessionConfig;
module ErrorHandler = ErrorHandler;
module DateUtils = DateUtils;
module Etag = Etag;

let listen =
    (
      ~inetAddr=Unix.inet_addr_any,
      port,
      serverConfig: ServerConfig.t('sessionData),
    ) => {
  let listenAddress = Unix.(ADDR_INET(inetAddr, port));
  let connectionHandler = Server.buildConnectionHandler(serverConfig);

  Lwt.async(() => {
    let%lwt _server = Lwt_io.establish_server_with_client_socket(
      listenAddress,
      connectionHandler,
    );
    ServerConfig.onListen(serverConfig, ());
    Lwt.return_unit;
  });

  Lwt.wait();
};

let listenAndWaitForever =
    (
      ~inetAddr=Unix.inet_addr_any,
      port,
      serverConfig: ServerConfig.t('sessionData),
    ) => {
  let (forever, _) = listen(~inetAddr, port, serverConfig);
  forever;
};