# juce_dmx
A Juce module to handle DMX / ArtNet / SACN protocol and devices

## Network input merging

Art-Net and sACN inputs offer a saved **Merge Mode** setting under **Input**:

- **HTP** (default): the highest level wins independently on each channel.
- **LTP**: the last changed value wins independently on each channel. Repeated
  packets with unchanged values do not take control from another source.

The receiver tracks multiple sources separately for each universe. Art-Net
sources are identified by IP address and physical port; sACN sources by CID.
For sACN, the highest effective priority wins first, then the selected merge
mode resolves sources with equal priority. Start code `0xDD` supplies per-slot
priorities and never channel levels. Priority zero in a per-slot packet releases
that slot; slots omitted from that packet are also released. Without per-slot
priorities, the source's universe priority applies (universe priority zero maps
to per-slot priority one). Other alternate start codes and preview packets are
ignored.

New sACN sources wait up to 1.5 seconds for their first per-slot priority packet
before using universe priority. Per-slot priorities expire after 2.5 seconds
without a refresh and fall back to universe priority. Sources expire after
2.5 seconds without level data for sACN, or 10 seconds for Art-Net; sACN stream
termination removes a source immediately. Remaining sources are merged again
even when no new packet arrives. When every source disappears, the last received
look is retained. Receiver reconfiguration clears source history.

Per-slot priority transmission is not currently exposed by the module.
