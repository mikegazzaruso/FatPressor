#include "PluginEditor.h"
#include "BinaryData.h"

FatPressorAudioProcessorEditor::FatPressorAudioProcessorEditor(FatPressorAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processorRef(p)
    // 1. Initialize RELAYS first
    , thresholdRelay(std::make_unique<juce::WebSliderRelay>("threshold"))
    , ratioRelay(std::make_unique<juce::WebSliderRelay>("ratio"))
    , attackRelay(std::make_unique<juce::WebSliderRelay>("attack"))
    , releaseRelay(std::make_unique<juce::WebSliderRelay>("release"))
    , fatRelay(std::make_unique<juce::WebSliderRelay>("fat"))
    , outputRelay(std::make_unique<juce::WebSliderRelay>("output"))
    , mixRelay(std::make_unique<juce::WebSliderRelay>("mix"))
{
    // 2. Create WebView with options and relays
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) { return getResource(url); })
            .withKeepPageLoadedWhenBrowserIsHidden()  // FL Studio compatibility

#if JUCE_WINDOWS && JUCE_USE_WIN_WEBVIEW2
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(
                    juce::File::tempDirectory).getChildFile("FatPressor_WebView")))
#endif
            // Register slider relays
            .withOptionsFrom(*thresholdRelay)
            .withOptionsFrom(*ratioRelay)
            .withOptionsFrom(*attackRelay)
            .withOptionsFrom(*releaseRelay)
            .withOptionsFrom(*fatRelay)
            .withOptionsFrom(*outputRelay)
            .withOptionsFrom(*mixRelay)

            // Native preset functions
            .withNativeFunction("loadPresetByIndex", [this](const juce::Array<juce::var>& args, auto complete) {
                DBG("[PluginEditor] loadPresetByIndex called with " + juce::String(args.size()) + " args");
                if (args.size() > 0 && args[0].isInt())
                {
                    int index = static_cast<int>(args[0]);
                    DBG("[PluginEditor] Loading preset index: " + juce::String(index));
                    processorRef.presetManager.loadPresetByIndex(index);
                }
                complete({});
            })
            .withNativeFunction("loadNextPreset", [this](const juce::Array<juce::var>&, auto complete) {
                DBG("[PluginEditor] loadNextPreset called");
                processorRef.presetManager.loadNextPreset();
                complete({});
            })
            .withNativeFunction("loadPreviousPreset", [this](const juce::Array<juce::var>&, auto complete) {
                DBG("[PluginEditor] loadPreviousPreset called");
                processorRef.presetManager.loadPreviousPreset();
                complete({});
            })
            .withNativeFunction("saveUserPreset", [this](const juce::Array<juce::var>& args, auto complete) {
                if (args.size() >= 2) {
                    juce::String name = args[0].toString();
                    juce::String category = args[1].toString();
                    bool success = processorRef.presetManager.saveUserPreset(name, category);
                    complete(juce::var(success));
                } else {
                    complete(juce::var(false));
                }
            })
            .withNativeFunction("deleteUserPreset", [this](const juce::Array<juce::var>& args, auto complete) {
                if (args.size() >= 1 && args[0].isInt()) {
                    int index = static_cast<int>(args[0]);
                    DBG("[PluginEditor] deleteUserPreset called with index: " + juce::String(index));
                    bool success = processorRef.presetManager.deleteUserPreset(index);
                    complete(juce::var(success));
                } else {
                    complete(juce::var(false));
                }
            })
            .withNativeFunction("getPresetList", [this](const juce::Array<juce::var>&, auto complete) {
                juce::Array<juce::var> presetArray;
                auto presets = processorRef.presetManager.getAllPresets();
                for (const auto& p : presets) {
                    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                    obj->setProperty("name", p.name);
                    obj->setProperty("category", p.category);
                    obj->setProperty("isFactory", p.isFactory);
                    presetArray.add(juce::var(obj.get()));
                }
                juce::DynamicObject::Ptr result = new juce::DynamicObject();
                result->setProperty("presets", presetArray);
                result->setProperty("currentIndex", processorRef.presetManager.getCurrentPresetIndex());
                complete(juce::var(result.get()));
            })
    );

    // 3. Create ATTACHMENTS after WebView exists
    auto* params = &processorRef.parameters;

    thresholdAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("threshold"), *thresholdRelay, nullptr);
    ratioAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("ratio"), *ratioRelay, nullptr);
    attackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("attack"), *attackRelay, nullptr);
    releaseAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("release"), *releaseRelay, nullptr);
    fatAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("fat"), *fatRelay, nullptr);
    outputAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("output"), *outputRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *params->getParameter("mix"), *mixRelay, nullptr);

    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    // Register as preset listener
    processorRef.presetManager.addListener(this);

    // Start metering timer (30 Hz)
    startTimerHz(30);

    // Size from mockup: 800x500
    setSize(800, 500);
}

FatPressorAudioProcessorEditor::~FatPressorAudioProcessorEditor()
{
    stopTimer();
    processorRef.presetManager.removeListener(this);
}

void FatPressorAudioProcessorEditor::timerCallback()
{
    // Push metering data to WebView
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputL", processorRef.inputLevelL.load());
    data->setProperty("inputR", processorRef.inputLevelR.load());
    data->setProperty("outputL", processorRef.outputLevelL.load());
    data->setProperty("outputR", processorRef.outputLevelR.load());
    data->setProperty("gr", processorRef.gainReduction.load());

    webView->emitEventIfBrowserIsVisible("metering", juce::var(data.get()));
}

void FatPressorAudioProcessorEditor::paint(juce::Graphics& g)
{
    // WebView handles all painting
    juce::ignoreUnused(g);
}

void FatPressorAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
FatPressorAudioProcessorEditor::getResource(const juce::String& url)
{
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    // Main HTML
    if (url == "/" || url == "/index.html")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };

    // CSS
    if (url == "/css/style.css")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::style_css, BinaryData::style_cssSize),
            juce::String("text/css")
        };

    // JavaScript
    if (url == "/js/main.js")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::main_js, BinaryData::main_jsSize),
            juce::String("application/javascript")
        };

    // JUCE WebView bridge (REQUIRED)
    if (url == "/js/juce/index.js")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("application/javascript")
        };

    // JUCE native interop check (REQUIRED)
    if (url == "/js/juce/check_native_interop.js")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::check_native_interop_js,
                      BinaryData::check_native_interop_jsSize),
            juce::String("application/javascript")
        };

    return std::nullopt;  // 404
}

// PresetManager::Listener callbacks
void FatPressorAudioProcessorEditor::presetChanged(const PresetManager::PresetInfo& newPreset)
{
    DBG("[PluginEditor] presetChanged called: " + newPreset.name);

    // Send preset change event to WebView
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("name", newPreset.name);
    data->setProperty("category", newPreset.category);
    data->setProperty("index", processorRef.presetManager.getCurrentPresetIndex());
    data->setProperty("isFactory", newPreset.isFactory);

    DBG("[PluginEditor] Emitting presetChanged event");
    webView->emitEventIfBrowserIsVisible("presetChanged", juce::var(data.get()));

    // Force sync all parameters to WebView UI
    DBG("[PluginEditor] Calling syncAllParametersToWebView");
    syncAllParametersToWebView();
}

void FatPressorAudioProcessorEditor::presetListChanged()
{
    // Send updated preset list to WebView
    sendPresetListToWebView();
}

void FatPressorAudioProcessorEditor::sendPresetListToWebView()
{
    juce::Array<juce::var> presetArray;
    auto presets = processorRef.presetManager.getAllPresets();

    for (const auto& p : presets)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("name", p.name);
        obj->setProperty("category", p.category);
        obj->setProperty("isFactory", p.isFactory);
        presetArray.add(juce::var(obj.get()));
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("presets", presetArray);
    data->setProperty("currentIndex", processorRef.presetManager.getCurrentPresetIndex());

    webView->emitEventIfBrowserIsVisible("presetList", juce::var(data.get()));
}

void FatPressorAudioProcessorEditor::syncAllParametersToWebView()
{
    DBG("[PluginEditor] syncAllParametersToWebView called");

    // Send all current parameter values to WebView
    // This forces UI knobs to update after preset load
    juce::DynamicObject::Ptr params = new juce::DynamicObject();

    auto addParam = [&](const juce::String& id) {
        if (auto* param = processorRef.parameters.getParameter(id))
        {
            float normalized = param->getValue();
            float scaled = param->convertFrom0to1(normalized);
            params->setProperty(id + "_normalized", normalized);
            params->setProperty(id + "_scaled", scaled);
            DBG("[PluginEditor] " + id + ": normalized=" + juce::String(normalized) + ", scaled=" + juce::String(scaled));
        }
        else
        {
            DBG("[PluginEditor] WARNING: Parameter not found: " + id);
        }
    };

    addParam("threshold");
    addParam("ratio");
    addParam("attack");
    addParam("release");
    addParam("fat");
    addParam("output");
    addParam("mix");

    DBG("[PluginEditor] Emitting parameterSync event");
    webView->emitEventIfBrowserIsVisible("parameterSync", juce::var(params.get()));
}
