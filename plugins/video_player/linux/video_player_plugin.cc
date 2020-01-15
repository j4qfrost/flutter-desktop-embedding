// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "plugins/video_player/linux/video_player_plugin.h"

#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <thread>

using std::string;
using std::cout;
using std::endl;

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>
#include <flutter/plugin_registrar_glfw.h>
#include <flutter/texture_registrar.h>

#include "ffmpeg/ffmpeg_manager.cc"
#include "ffmpeg/ffmpeg_texture.cc"

namespace plugins_video_player {

namespace {
// See video_player.dart for documentation.
const char kChannelName[] = "flutter.io/videoPlayer";
const char kInitMethod[] = "init";
const char kCreateMethod[] = "create";
const char kPlayMethod[] = "play";
const char kSetLoopingMethod[] = "setLooping";
const char kSetVolumeMethod[] = "setVolume";
const char kPauseMethod[] = "pause";
const char kPositionMethod[] = "position";
const char kDisposeMethod[] = "dispose";
}

using flutter::EncodableMap;
using flutter::EncodableValue;

typedef flutter::MethodResult<EncodableValue> FlutterResponderEV;
typedef flutter::MethodChannel<EncodableValue> FlutterMethdodChannelEV;
typedef flutter::MethodCall<EncodableValue> FlutterMethdodCallEV;

class VideoPlayerPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  virtual ~VideoPlayerPlugin();

  string GetAssetURIFromArgs(const EncodableValue& arguments) const;

 protected:
  void Create(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result);
  void Play(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result);
  void Pause(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result);
  void Position(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result);
  void Dispose(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result);

 private:
  // Creates a plugin that communicates on the given channel.
  VideoPlayerPlugin(
      std::unique_ptr<FlutterMethdodChannelEV> channel);

  // Called when a method is called on |channel_|;
  void HandleMethodCall(
    const FlutterMethdodCallEV &method_call, std::unique_ptr<FlutterResponderEV> result);
  void HandleListener(
    const FlutterMethdodCallEV &method_call, std::unique_ptr<FlutterResponderEV> result, 
    const string& channel_name, const string& uri);
  // The MethodChannel used for communication with the Flutter engine.
  std::unique_ptr<FlutterMethdodChannelEV> channel_;

  static flutter::TextureRegistrar* texture_registrar;
  static flutter::BinaryMessenger* messenger;

  std::unordered_map<int64_t, FFMPEGManager*>* managers_by_texture_id;
  std::unordered_map<FFMPEGManager*, std::vector<int64_t>*>* texture_ownership;
  std::unordered_map<string, FFMPEGManager*>* managers_by_uri;
  // Private implementation.
};

flutter::TextureRegistrar* VideoPlayerPlugin::texture_registrar = NULL;
flutter::BinaryMessenger* VideoPlayerPlugin::messenger = NULL;

// static
void VideoPlayerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  messenger = registrar->messenger();
  texture_registrar = registrar->textures();
  auto channel = std::make_unique<FlutterMethdodChannelEV>(
      messenger, kChannelName,
      &flutter::StandardMethodCodec::GetInstance());
  auto *channel_pointer = channel.get();

  // Uses new instead of make_unique due to private constructor.
  std::unique_ptr<VideoPlayerPlugin> plugin(
      new VideoPlayerPlugin(std::move(channel)));

  channel_pointer->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerPlugin::VideoPlayerPlugin(
    std::unique_ptr<FlutterMethdodChannelEV> channel)
    : channel_(std::move(channel)) {
  managers_by_texture_id = new std::unordered_map<int64_t, FFMPEGManager*>();
  texture_ownership = new std::unordered_map<FFMPEGManager*, std::vector<int64_t>*>();
  managers_by_uri = new std::unordered_map<string, FFMPEGManager*>();
}

VideoPlayerPlugin::~VideoPlayerPlugin() {
    for(std::unordered_map<FFMPEGManager*, std::vector<int64_t>*>::iterator itr = texture_ownership->begin(); itr != texture_ownership->end(); itr++)
    {
        delete itr->first;
        delete itr->second;
    }
}

EncodableValue GrabEncodableValueFromArgs(const EncodableValue& arguments, const char* key) {
  EncodableMap arg_map = arguments.MapValue();
  auto it = arg_map.find(EncodableValue(key));
  if (it != arg_map.end()) {
    return it->second;
  }
  return EncodableValue();
}

string VideoPlayerPlugin::GetAssetURIFromArgs(const EncodableValue& arguments) const {
  EncodableValue uri = GrabEncodableValueFromArgs(arguments, "uri");
  if (!uri.IsString()) {
    uri = GrabEncodableValueFromArgs(arguments, "asset");
    if (!uri.IsString()) {
      string result = "";
      return result;
    }
  }
  return uri.StringValue();
}

void VideoPlayerPlugin::Create(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result) {
  string uri_val = GetAssetURIFromArgs(arguments);
  if (uri_val == "") {
    result->Error("Asset arguments do not exist");
    return;
  }

  FFMPEGManager *fman;
  auto it = managers_by_uri->find(uri_val);
  if (it == managers_by_uri->end()) {
    fman = new FFMPEGManager();
    managers_by_uri->insert({uri_val, fman});

    std::vector<int64_t> *list = new std::vector<int64_t>();
    texture_ownership->insert({fman, std::move(list)});
  }
  else {
    fman = it->second;
  }

  Texture* texture = new FFMPEGTexture(fman);
  int64_t texture_id = texture_registrar->RegisterTexture(std::move(texture));
  managers_by_texture_id->insert({texture_id, fman});
  auto owner = texture_ownership->find(fman);
  owner->second->push_back(texture_id);

  char channel_name[256];
  sprintf(channel_name, "%s/videoEvents%ld", kChannelName, texture_id);
  auto channel = std::make_unique<FlutterMethdodChannelEV>(
      messenger, channel_name,
      &flutter::StandardMethodCodec::GetInstance());
  auto *channel_pointer = channel.get();

  EncodableMap encodables = {
    {EncodableValue("textureId"), EncodableValue(texture_id)},
  };
  EncodableValue value(encodables);

  channel_pointer->SetMethodCallHandler(
      [plugin_pointer = this, channel_name, uri_val](const auto &call, auto result) {
        plugin_pointer->HandleListener(call, std::move(result), channel_name, uri_val);
      });

  result->Success(&value);
}

void VideoPlayerPlugin::Play(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result) {
  int64_t texture_id = GrabEncodableValueFromArgs(arguments, "textureId").LongValue();
  FFMPEGManager *fman = managers_by_texture_id->find(texture_id)->second;
  std::vector<int64_t> *texture_ids = texture_ownership->find(fman)->second;

  std::thread t(&FFMPEGManager::Loop, fman, [texture_ids, tr=std::move(texture_registrar)]() {
    for (auto &&id : *texture_ids)
    {
      tr->MarkTextureFrameAvailable(id);
      printf("asdad");
    }
  });
  
  t.detach();
  result->Success();
}

void VideoPlayerPlugin::Pause(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result) {
  result->Success();
}

void VideoPlayerPlugin::Position(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result) {
  result->Success();
}

void VideoPlayerPlugin::Dispose(const EncodableValue& arguments, std::unique_ptr<FlutterResponderEV> result) {
  result->Success();
}

void VideoPlayerPlugin::HandleListener(
    const FlutterMethdodCallEV &method_call,
    std::unique_ptr<FlutterResponderEV> result,
    const string& channel_name,
    const string& uri) {
  string method_name = method_call.method_name();
  cout << "Method called: " << method_name << endl;
  if (method_name.compare("listen") == 0) {
    managers_by_uri->find(uri)->second->Init(uri.c_str(), AV_PIX_FMT_RGBA, 1280, 720);
    EncodableMap encodables = {
      {EncodableValue("event"), EncodableValue("initialized")},
      {EncodableValue("duration"), EncodableValue(1)},
      {EncodableValue("width"), EncodableValue(1280)},
      {EncodableValue("height"), EncodableValue(720)},
    };
    EncodableValue value(encodables);
    std::unique_ptr<std::vector<uint8_t>> message = flutter::StandardMethodCodec::GetInstance().EncodeSuccessEnvelope(&value);

    messenger->Send(channel_name, std::move(&(*message)[0]), message->size());

    result->Success();
  } else if (method_name.compare("cancel") == 0) {
    result->Success();
  } else {
    result->NotImplemented();
  }
}

void VideoPlayerPlugin::HandleMethodCall(
    const FlutterMethdodCallEV &method_call,
    std::unique_ptr<FlutterResponderEV> result) {
  if (!method_call.arguments() || method_call.arguments()->IsNull()) {
    result->Error("Bad Arguments", "Null arguments received");
    return;
  }

  string method_name = method_call.method_name();
  cout << "Method called: " << method_name << endl;
  if (method_name.compare(kInitMethod) == 0) {
    result->Success();
  } else if (method_name.compare(kCreateMethod) == 0) {
    Create(*method_call.arguments(), std::move(result));
  } else if (method_name.compare(kPlayMethod) == 0) {
    Play(*method_call.arguments(), std::move(result));
  } else if (method_name.compare(kSetVolumeMethod) == 0) {
    result->Success();
  } else if (method_name.compare(kSetLoopingMethod) == 0) {
    result->Success();
  } else if (method_name.compare(kPauseMethod) == 0) {
    Pause(*method_call.arguments(), std::move(result));
  } else if (method_name.compare(kPositionMethod) == 0) {
    Position(*method_call.arguments(), std::move(result));
  } else if (method_name.compare(kDisposeMethod) == 0) {
    Dispose(*method_call.arguments(), std::move(result));
  } else {
    result->NotImplemented();
  }
}
}  // namespace plugins_video_player

void VideoPlayerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  // The plugin registrar owns the plugin, registered callbacks, etc., so must
  // remain valid for the life of the application.
  static auto *plugin_registrar = new flutter::PluginRegistrarGlfw(registrar);

  plugins_video_player::VideoPlayerPlugin::RegisterWithRegistrar(
      plugin_registrar);
}
