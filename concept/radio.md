1. Die Kernfunktionen
A. Sendersuche & Management

    Automatischer Suchlauf (Scan): Scannt Frequenzen (DAB+) oder parst Provider-Listen (Internetradio).

    Manuelle Abstimmung: Feinjustierung oder direkte Eingabe einer URL/Frequenz.

    DLS/Radiotext: Anzeige von Zusatzinfos wie "Aktueller Song", "Interpret" oder "Nachrichten".

B. Favoriten-System (Presets)

    Schnellwahltasten: Große, leicht zu treffende Buttons für die Top 5–10 Sender.

    Speicherfunktion: Langes Drücken auf einen Button speichert den aktuellen Sender.

C. Audio-Visualisierung & Metadaten

    Station Logo: Anzeige des Senderlogos (PNG/SVG).

    Signalstärke: Eine kleine Anzeige für die Empfangsqualität.

    Playback-Control: Play/Pause und Mute (Stummschalten).

2. Technisches Konzept (Backend)

Für das Radio-Modul schlage ich vor, auf GStreamer zu setzen. Das ist der Standard unter Linux für Multimedia.

    Internetradio: Wir füttern GStreamer mit einer URL (z.B. http://stream.antenne.de).

    DAB+: Wir nutzen ein Tool wie welle.io oder ein Kommandozeilen-Tool (dab-rtlsdr), das den Stream an GStreamer übergibt.

3. UI-Entwurf (Frontend)

Da du das Glas-Design nutzt, sollte das Radio-Modul so aufgebaut sein:

    Oben: Große Anzeige des Sendernamens und des aktuellen Titels (Metadaten).

    Mitte: Senderlogo (zentriert) oder ein grafischer Equalizer.

    Unten: Eine horizontale Reihe mit Favoriten-Buttons (Presets).

4. Entwurf der Radio-Klasse (RadioManager)

Ähnlich wie beim BluetoothManager sollten wir die Logik kapseln. Hier ein erster Entwurf, was die Klasse leisten muss:
C++

class RadioManager {
public:
    void play_station(const std::string& url);
    void stop();
    void set_volume(int volume);
    
    // Signale für das UI
    void on_metadata_changed(const std::string& title, const std::string& artist);
    void on_station_logo_found(const std::string& pixbuf_path);
};

Strategische Entscheidung: Internetradio oder DAB+?

Bevor wir den Code schreiben:

    Internetradio: Wir können sofort starten, da wir nur WLAN/Internet brauchen. Wir nutzen eine Liste von .m3u oder .pls Streams.

    DAB+: Hast du bereits einen RTL-SDR USB-Stick für den Empfang? Wenn nein, empfehle ich, erst mit Internetradio zu starten und die Logik später auf DAB+ zu erweitern.
