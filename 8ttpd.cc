#include <v8.h>
#include <string>
#include <map>
using namespace v8;
using namespace std;

extern "C" {

#include <err.h>
#include <stdlib.h>
#include <sys/queue.h> // needed by event.h -- wtf?
#include <event.h>
#include <evhttp.h>

Handle<String> ReadFile(const string& name) {
  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) return Handle<String>();
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);
  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  Handle<String> result = String::New(chars, size);
  delete[] chars;
  return result;
}

static void
callback(struct evhttp_request *req, void *fileName) {
  // No, creating the whole V8 thingy on every request is not the last word.
  HandleScope handle_scope;
  Persistent<Context> context = Context::New();
  Context::Scope context_scope(context);
  Handle<String> source = ReadFile((char *) fileName);
  Handle<Script> script = Script::Compile(source);
  Handle<Value> result = script->Run();
  context.Dispose();
  String::AsciiValue ascii(result);
  evbuffer_add_printf(req->output_buffer, "%s\n", *ascii);
  evhttp_send_reply(req, HTTP_OK, "OK", NULL);
}

int
main(int argc, char **argv) {
  struct evhttp *httpd;
  char *addr = "127.0.0.1";
  u_short port = 8080;
  if (!event_init())                       errx(EXIT_FAILURE, "Cannot initialize libevent");
  if (!(httpd = evhttp_start(addr, port))) errx(EXIT_FAILURE, "Cannot create HTTP server");
  evhttp_set_gencb(httpd, callback, argv[1]);
  event_loop(0);
}

}
