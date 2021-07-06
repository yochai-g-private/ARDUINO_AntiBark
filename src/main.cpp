#include "StdIR.h"
#include "Logger.h"
#include "RGB_Led.h"
#include "Timer.h"
#include "Random.h"
#include "Cfg.h"

enum    Pins
{
    IR_PIN          = 2,
    LED_RED         = 4,
    LED_GREEN       = 6,
    LED_BLUE        = 8,
    SPEAKER_PIN     = 11,
};

StdIR::Receiver     IR(IR_PIN);
RGB_Led             RGB(LED_RED, LED_GREEN, LED_BLUE);
ToneOutputPin       speaker(SPEAKER_PIN);
Timer               timer;
Timer               IR_mode_timer;
#define             IR_mode     IR_mode_timer.IsStarted()

void restore_to_factory_defaults();
void switch_to_functional_mode();
void start_timer();
void update_leds();
void toggle_speaker();
void check_IR();
void load();
void store();

enum 
{
    __MIN_FREQUENCY_kHz = 15,
    __MAX_FREQUENCY_kHz = 65,
    FRQUENCY_STEP_kHz   = 5,

    DEFAULT_MAX_TIME_INTERVAL_SECONDS = 30,
};

uint32_t    MIN_TIME_INTERVAL_SECONDS =  1,
            MAX_TIME_INTERVAL_SECONDS;

uint32_t    MIN_FREQUENCY_kHz,
            MAX_FREQUENCY_kHz;

class MyCfg : public Cfg
{
protected:

    MyCfg() : Cfg("Anti-Bark", 1)   {   }

    class Frequency : public Leaf
    {
    public:

        Frequency(const char* _name, uint32_t& _value) : name(_name), value(_value)     {   }

        const char* GetName()       const   { return name; }

        bool SetValue(String& s)            { return convert(s.c_str(), value); }

        uint32_t getSize()                              const   { return sizeof(value); }
        void*    getData()                              const   { return &value; }
        String   ToString()                             const   { return Cfg::Leaf::ToString((unsigned long&)value); }

        const char* name;
        uint32_t&   value;
    };

    static Frequency    lowest_freq;
    static Frequency    highest_freq;
    static Frequency    interval;

    class MyRoot : public Root
    {
        DECLARE_CFG_NODE_ITERATOR_FUNCS_3(lowest_freq, highest_freq, interval);
    };

    MyRoot root;

    Root& GetRoot()
    {
        return root;
    }

public:

    static MyCfg instance;
};

MyCfg::Frequency MyCfg::lowest_freq ("lowest",   MIN_FREQUENCY_kHz);
MyCfg::Frequency MyCfg::highest_freq("highest",  MAX_FREQUENCY_kHz);
MyCfg::Frequency MyCfg::interval    ("interval", MAX_TIME_INTERVAL_SECONDS);

MyCfg            MyCfg::instance;

//==========================================================
void setup() 
{
    Logger::Initialize();

    restore_to_factory_defaults();

    load();

    IR.Begin();

    switch_to_functional_mode();
}
//==========================================================
void loop() 
{
    if(IR_mode_timer.Test())
    {
        switch_to_functional_mode();
        return;
    }

    toggle_speaker();
    check_IR();
}
//==========================================================
void restore_to_factory_defaults()
{
    MIN_FREQUENCY_kHz         = __MIN_FREQUENCY_kHz + FRQUENCY_STEP_kHz;
    MAX_FREQUENCY_kHz         = __MAX_FREQUENCY_kHz;
    MAX_TIME_INTERVAL_SECONDS = DEFAULT_MAX_TIME_INTERVAL_SECONDS;
}
//==========================================================
void switch_to_functional_mode()
{
    speaker.Quiet();
    
    IR_mode_timer.Stop();
    start_timer();
    update_leds();
}
//==========================================================
#define get_random(minval, maxval)  ((Random::Get() % (maxval - minval + 1)) + minval)
//==========================================================
void start_timer()
{
    uint32_t seconds = get_random(MIN_TIME_INTERVAL_SECONDS, MAX_TIME_INTERVAL_SECONDS);
    timer.StartOnce(seconds * 1000);
    LOGGER << "Timer triggered for" << seconds << " seconds" << NL;
}
//==========================================================
void update_leds()
{
    RGB.SetOff();
    IDigitalOutput& led = (speaker.IsQuiet()) ? RGB.GetBlue() : RGB.GetGreen();
    led.On();
}
//==========================================================
void play_sound(uint32_t freq_kHz)
{
    speaker.Tone(freq_kHz * 1000); 
    LOGGER << "Playing tone " << freq_kHz << " kHz" << NL;
}
//==========================================================
void toggle_speaker()
{
    if(IR_mode)
        return;

    if(!timer.Test())
        return;

    if(speaker.IsQuiet())
    {
        uint32_t frequency = get_random(MIN_FREQUENCY_kHz, MAX_FREQUENCY_kHz);
        play_sound(frequency);
    }
    else
    {
        speaker.Quiet();
        LOGGER << "Silent" << NL;
    }

    update_leds();
    start_timer();
}
//==========================================================
void start_IR_timer()
{
    IR_mode_timer.StartOnce(30 * 1000);
}
//==========================================================
void check_IR()
{
    StdIR::Key ir_key;
    
    if(!IR.Recv(ir_key))
        return;

    bool proceed_IR_key(StdIR::Key ir_key);

    bool Ok = proceed_IR_key(ir_key);

    if(IR_mode)
    {
        IDigitalOutput& led = (Ok) ? RGB.GetGreen() : RGB.GetRed();

        led.On();
        delay(250);
        led.Off();

        start_IR_timer();

        return;
    }

    switch_to_functional_mode();
}
//==========================================================
enum IR_STATUS
{
    MAIN,
    SET_LOWEST,
    SET_HIGHEST,
    FACTORY_RESTORE,
};

IR_STATUS     ir_status;
//==========================================================
bool change_frequency(uint32_t& freq, int add, uint32_t margin)
{
    add *= FRQUENCY_STEP_kHz;

    if(margin == (freq + add))
        return false;

    freq += add;
    play_sound(freq);
    store();

    return true;   
}
//==========================================================
bool proceed_IR_key(StdIR::Key ir_key)
{
    static bool human = false;

    if(!IR_mode)
    {
        if(StdIR::OK == ir_key)
        {
            speaker.Quiet();
            RGB.SetOff();
            timer.Stop();

            start_IR_timer();
            
            ir_status   = MAIN;
            human       = false;

            return true;
        }

        return false;
    }

    switch(ir_key)
    {
        case StdIR::OK      : 
            switch(ir_status)
            {
                case MAIN           :   IR_mode_timer.Stop();
                                        return true;
                case SET_LOWEST     :   
                case SET_HIGHEST    :   
                case FACTORY_RESTORE: speaker.Quiet(); 
                                        ir_status = MAIN;   
                                        return true;
            }

            return false;

        case StdIR::LEFT        :   ir_status = SET_LOWEST;
                                    play_sound(MIN_FREQUENCY_kHz);
                                    return true;

        case StdIR::RIGHT       :   ir_status = SET_HIGHEST;
                                    play_sound(MAX_FREQUENCY_kHz);
                                    return true;
        
        case StdIR::UP          :   
            switch(ir_status)
            {
                case SET_LOWEST     :   return change_frequency(MIN_FREQUENCY_kHz, 1, MAX_FREQUENCY_kHz);
                case SET_HIGHEST    :   return change_frequency(MAX_FREQUENCY_kHz, 1, __MAX_FREQUENCY_kHz+1);
            }
            
            return false;

        case StdIR::DOWN        :   
            switch(ir_status)
            {
                case SET_LOWEST     :   return change_frequency(MIN_FREQUENCY_kHz, -1, __MIN_FREQUENCY_kHz-1);
                case SET_HIGHEST    :   return change_frequency(MAX_FREQUENCY_kHz, -1, MIN_FREQUENCY_kHz);
            }
            
            return false;

        case StdIR::STAR        :
            switch(ir_status)
            {
                case MAIN           :   ir_status = FACTORY_RESTORE; 
                                        return true;
                case FACTORY_RESTORE:   ir_status = MAIN; 
                                        return true;
            }
            
            return false;
            
        case StdIR::DIEZ        :
            switch(ir_status)
            {
                case MAIN           :   human = !human; 
                                        
                                        if(human)
                                            speaker.Tone(440);
                                        else
                                            speaker.Quiet();

                                        return true;

                case FACTORY_RESTORE:   restore_to_factory_defaults();
                                        ir_status = MAIN;
                                        return true;
            }
            
            return false;

        case StdIR::N0          :   MAX_TIME_INTERVAL_SECONDS = DEFAULT_MAX_TIME_INTERVAL_SECONDS;
                                    return true;

        #define TREATE_NUMKEY( n )  case StdIR::N##n : MAX_TIME_INTERVAL_SECONDS = n * 10;  return true

        TREATE_NUMKEY( 1 );
        TREATE_NUMKEY( 2 );
        TREATE_NUMKEY( 3 );
        TREATE_NUMKEY( 4 );
        TREATE_NUMKEY( 5 );
        TREATE_NUMKEY( 6 );
        TREATE_NUMKEY( 7 );
        TREATE_NUMKEY( 8 );
        TREATE_NUMKEY( 9 );

        #undef  TREATE_NUMKEY
    }
}
//==========================================================
void load()             { MyCfg::instance.Load(); }
//==========================================================
void store()            { MyCfg::instance.Store(); }
//==========================================================
