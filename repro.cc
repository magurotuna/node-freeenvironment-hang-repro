// create(source): creates an Environment on the isolate, IsolateData, and
// event loop of the calling Environment, runs `source` in it, and returns an
// id. free(id): calls node::FreeEnvironment() on that Environment.
#include <node.h>

#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<node::Environment*> envs;

void Create(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  node::Environment* parent =
      node::GetCurrentEnvironment(isolate->GetCurrentContext());
  v8::Local<v8::Context> context = node::NewContext(isolate);
  node::Environment* env =
      node::CreateEnvironment(node::GetEnvironmentIsolateData(parent),
                              context,
                              {"node"},
                              {},
                              node::EnvironmentFlags::kNoCreateInspector,
                              node::AllocateEnvironmentThreadId());
  v8::String::Utf8Value source(isolate, args[0]);
  {
    v8::Context::Scope context_scope(context);
    (void)node::LoadEnvironment(env,
                                std::string_view(*source, source.length()));
  }
  envs.push_back(env);
  args.GetReturnValue().Set(static_cast<uint32_t>(envs.size() - 1));
}

void Free(const v8::FunctionCallbackInfo<v8::Value>& args) {
  uint32_t id = args[0].As<v8::Uint32>()->Value();
  node::FreeEnvironment(envs[id]);
  envs[id] = nullptr;
}

}  // namespace

NODE_MODULE_INIT(/* exports, module, context */) {
  NODE_SET_METHOD(exports, "create", Create);
  NODE_SET_METHOD(exports, "free", Free);
}
