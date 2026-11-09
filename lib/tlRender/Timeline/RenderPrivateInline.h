// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

namespace tl
{
#if defined(TLRENDER_OCIO)
    template<typename T>
    inline OCIOCache<T>::OCIOCache()
    {
        // The key for the options as they start out, so that anything
        // drawn before the first setOptions() is keyed the same way as it
        // would be after being set to the same value.
        _key = getOCIOOptionsKey(_options);
        _keys.push_front(_key);
    }

    template<typename T>
    inline const OCIOOptions& OCIOCache<T>::getOptions() const
    {
        return _options;
    }

    template<typename T>
    inline const std::string& OCIOCache<T>::getKey() const
    {
        return _key;
    }

    template<typename T>
    inline std::vector<std::string> OCIOCache<T>::setOptions(const OCIOOptions& value)
    {
        std::vector<std::string> out;
        _options = value;
        _key = getOCIOOptionsKey(_options);
        const auto i = std::find(_keys.begin(), _keys.end(), _key);
        if (i != _keys.end())
        {
            _keys.erase(i);
        }
        _keys.push_front(_key);
        while (_keys.size() > _keysMax)
        {
            // What is built holds textures, and the shaders are compiled,
            // so what is no longer being drawn through is let go of.
            const std::string prefix = _keys.back() + '\n';
            auto j = _data.lower_bound(prefix);
            while (j != _data.end() &&
                0 == j->first.compare(0, prefix.size(), prefix))
            {
                j = _data.erase(j);
            }
            out.push_back(_keys.back());
            _keys.pop_back();
        }
        return out;
    }

    template<typename T>
    inline std::shared_ptr<T> OCIOCache<T>::get(
        const std::string& input,
        const std::function<void(T&, const OCIOOptions&)>& init,
        const std::function<void(const std::string&)>& error)
    {
        const std::string key = _key + '\n' + input;
        auto i = _data.find(key);
        if (i == _data.end())
        {
            auto data = std::make_shared<T>();
            try
            {
                OCIOOptions options = _options;
                options.input = input;
                init(*data, options);
            }
            catch (const std::exception& e)
            {
                if (error)
                {
                    error(e.what());
                }
                data.reset();
            }
            i = _data.insert(std::make_pair(key, data)).first;
        }
        return i->second;
    }

    template<typename T>
    inline void OCIOCache<T>::setInputResolver(
        const std::function<std::string(
            const std::string& path,
            const ftk::ImageTags&)>& value)
    {
        _resolver = value;
        _inputs.clear();
    }

    template<typename T>
    inline std::string OCIOCache<T>::getLayerInput(
        const std::string& layerInput,
        const std::string& path,
        const std::shared_ptr<ftk::Image>& image)
    {
        std::string out = layerInput;
        if (out.empty() && _resolver && !path.empty())
        {
            const std::string key = _key + '\n' + path;
            auto i = _inputs.find(key);
            if (i == _inputs.end())
            {
                i = _inputs.insert(std::make_pair(
                    key,
                    _resolver(path, image ? image->getTags() : ftk::ImageTags()))).first;
            }
            out = i->second;
        }
        return out;
    }

    template<typename T>
    inline std::vector<std::pair<std::string, std::string> > OCIOCache<T>::getLayerInputs(
        const VideoFrame& videoFrame,
        const std::string& itemInput)
    {
        std::vector<std::pair<std::string, std::string> > out;
        if (_options.enabled &&
            !_options.display.empty() &&
            !_options.view.empty())
        {
            const std::string input = !itemInput.empty() ? itemInput : _options.input;
            for (const auto& layer : videoFrame.layers)
            {
                std::string a = getLayerInput(layer.ocioInput, layer.path, layer.image);
                if (a.empty())
                {
                    a = input;
                }
                std::string b = getLayerInput(layer.ocioInputB, layer.pathB, layer.imageB);
                if (b.empty())
                {
                    b = input;
                }
                out.push_back(std::make_pair(a, b));
            }
        }
        return out;
    }
#endif // TLRENDER_OCIO
}
