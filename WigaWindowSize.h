#pragma once
#include <juce_core/juce_core.h>

// NINE HANDS | DSP : taille par defaut des nouvelles fenetres du plugin, commune a toutes ses instances
// (clic droit > Plugin size > Default size (new windows)). Reglage enregistre dans le dossier de l'utilisateur :
//   Windows : %APPDATA%\NINE HANDS DSP\<Plugin>\settings.xml
//   macOS   : ~/Library/Application Support/NINE HANDS DSP/<Plugin>/settings.xml
namespace WigaWindowSize
{
    inline juce::File file (const juce::String& plugin)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NINE HANDS DSP").getChildFile (plugin).getChildFile ("settings.xml");
    }
    inline int defaultIndex (const juce::String& plugin, int numSizes)
    {
        const auto f = file (plugin);
        if (f.existsAsFile())
            if (auto x = juce::XmlDocument::parse (f))
                return juce::jlimit (0, juce::jmax (0, numSizes - 1), x->getIntAttribute ("defaultSize", 0));
        return 0;
    }
    inline void setDefaultIndex (const juce::String& plugin, int idx)
    {
        const auto f = file (plugin);
        juce::XmlElement x ("NineHandsSettings");
        if (f.existsAsFile())
            if (auto old = juce::XmlDocument::parse (f))
                x = *old;
        x.setAttribute ("defaultSize", juce::jmax (0, idx));
        f.getParentDirectory().createDirectory();
        x.writeTo (f);
    }
}
