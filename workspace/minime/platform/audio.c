#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "traits.h"
#include "utils.h"

int MINIME_audioJackConnected(void) {
    const MinimeTraits *traits = MINIME_traits();

    return (traits && MINIME_traitAvailable(traits->jack_state_path))
               ? getInt((char *)traits->jack_state_path)
               : 0;
}

void MINIME_audioSetRawVolume(int value) {
    const MinimeTraits *traits = MINIME_traits();
    char command[512];

    if (!traits)
        return;
    if (strcmp(traits->sound_card, "default") == 0)
        snprintf(command, sizeof(command), "amixer -q sset '%s' %d%% >/dev/null 2>&1",
                 traits->sound_mixer, value);
    else
        snprintf(command, sizeof(command), "amixer -q -c '%s' sset '%s' %d%% >/dev/null 2>&1",
                 traits->sound_card, traits->sound_mixer, value);
    (void)system(command);
}
