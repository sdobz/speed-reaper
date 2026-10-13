# REAPER API subset for the Speed Editor plugin

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Token-efficient extraction of the REAPER API function list from
`docs/reaper-api.htm` (source: REAPER v7.82/win64). Curated for this project:
transport / playback state, edit cursor, tempo & time maps, actions,
control surface (CSurf), view/zoom/scroll, project & markers, undo,
and the ReaScript runtime helpers used for prototyping.

## How to use

- Every function name below is also a **REAPER SDK host API**: in the C++
  plugin fetch it with `rec->GetFunc("FunctionName")` — signatures match
  `vendor/reaper-sdk/sdk/reaper_plugin_functions.h`. This doc is therefore
  also the catalog of the transport-related host APIs (see
  `docs/reaper-plugin.md` §3–§4 for which ones the plugin uses).
- Signatures shown are the C form; EEL2/Lua/Python variants of every
  function remain in the full `docs/reaper-api.htm` (search the function
  name — each entry has a `#Name` anchor).
- `ReaProject* proj` parameters: pass `nullptr` for the active project.
- Entries with no description are self-evident in the full doc.

## Sections

## 1. Transport & playback

- **GetPlayState** — C: `int GetPlayState()`
  &1=playing, &2=paused, &4=is recording
- **GetPlayStateEx** — C: `int GetPlayStateEx(ReaProject* proj)`
  &1=playing, &2=paused, &4=is recording
- **GetPlayPosition** — C: `double GetPlayPosition()`
  returns latency-compensated actual-what-you-hear position
- **GetPlayPositionEx** — C: `double GetPlayPositionEx(ReaProject* proj)`
  returns latency-compensated actual-what-you-hear position
- **GetPlayPosition2** — C: `double GetPlayPosition2()`
  returns position of next audio block being processed
- **GetPlayPosition2Ex** — C: `double GetPlayPosition2Ex(ReaProject* proj)`
  returns position of next audio block being processed
- **GetAllProjectPlayStates** — C: `int GetAllProjectPlayStates(ReaProject* ignoreProject)`
  returns the bitwise OR of all project play states (1=playing, 2=pause, 4=recording)
- **GetSetProjectInfo** — C: `double GetSetProjectInfo(ReaProject* project, const char* desc, double value, bool is_set)`
  Get or set project information.
  RENDER_SETTINGS : (&(1|2)==0)=master mix, &1=stems+master mix, &2=stems only, &4=multichannel tracks to multichannel files, &8=use render matrix, &16=tracks with only mono media to mono files, &32=selected media items, &64=selected media items via master, &128=selected tracks via master, &256=embed transients if format supports, &512=embed metadata if format supports, &1024=embed take markers if format supports, &2048=2nd pass render, &4096=render razor edits, &8192=pre-fader stems (not if via master), &16384=only stem channels sent to parent, &32768=preserve source metadata if possible, &(1<<16)=preserve source start offset if possible, &(2<<16)=preserve source media sample rate if possible, &(4<<16)=if rendering selected items or razor edits, render as a single file, &(8<<16)=parallel render via master, &(16<<16)=delay render start to allow FX to initialize and load samples
  RENDER_BOUNDSFLAG : 0=custom time bounds, 1=entire project, 2=time selection, 3=all project regions, 4=selected media items, 5=selected project regions, 6=all project markers, 7=selected project markers
  RENDER_CHANNELS : number of channels in rendered file
  RENDER_SRATE : sample rate of rendered file (or 0 for project sample rate)
  RENDER_STARTPOS : render start time when RENDER_BOUNDSFLAG=0
  RENDER_ENDPOS : render end time when RENDER_BOUNDSFLAG=0
  RENDER_TAILFLAG : apply render tail setting when rendering: &1=custom time bounds, &2=entire project, &4=time selection, &8=all project markers/regions, &16=selected media items, &32=selected project markers/regions
  RENDER_TAILMS : tail length in ms to render (only used if RENDER_BOUNDSFLAG and RENDER_TAILFLAG are set)
  RENDER_ADDTOPROJ : &1=add rendered files to project, &2=do not render files that are likely silent
  RENDER_DITHER : &1=dither, &2=noise shaping, &4=dither stems, &8=noise shaping on stems, &16=disable all
  RENDER_NORMALIZE : &1=enable normalization, (&14==0)=LUFS-I, (&14==2)=RMS, (&14==4)=peak, (&14==6)=true peak, (&14==8)=LUFS-M max, (&14==10)=LUFS-S max, &16=adjust mono media -3dB, &(16|(8<<16))=adjust mono media +3dB, (&(32|4096|(16<<16))==32)=normalize as if files play together, (&(32|4096|(16<<16))==4096)=normalize to loudest file, (&(32|4096|(16<<16))==(32|4096))=normalize as if files play together (common gain), (&(32|4096|(16<<16))==(16<<16))=normalize to master mix, &64=enable brickwall limit, &128=brickwall limit true peak, (&(256|2048)==256)=only normalize files that are too loud, (&(256|2048)==2048)=only normalize files that are too quiet, &512=apply fade-in, &1024=apply fade-out, &16384=trim starting silence, &32768=trim ending silence, &(1<<16)=pad start with silence, &(2<<16)=pad end with silence, &(4<<16)=disable all render postprocessing, (&((32<<16)|(64<<16))==(32<<16))=limit as if files play together, (&((32<<16)|(64<<16))==(64<<16))=limit to master mix
  RENDER_NORMALIZE_TARGET : render normalization target (0.5 means -6.02dB, requires RENDER_NORMALIZE&1)
  RENDER_BRICKWALL : render brickwall limit (0.5 means -6.02dB, requires RENDER_NORMALIZE&64)
  RENDER_FADEIN : render fade-in (0.001 means 1 ms, requires RENDER_NORMALIZE&512)
  RENDER_FADEOUT : render fade-out (0.001 means 1 ms, requires RENDER_NORMALIZE&1024)
  RENDER_FADEINSHAPE : render fade-in shape
  RENDER_FADEOUTSHAPE : render fade-out shape
  RENDER_FADELPF : render low pass frequency fade, &1=fade-in, &2=fade-out
  RENDER_PADSTART : pad render start with silence (0.001 means 1ms, requires RENDER_NORMALIZE&(1<<16))
  RENDER_PADEND : pad render end with silence (0.001 means 1ms, requires RENDER_NORMALIZE&(2<<16))
  RENDER_TRIMSTART : trim render start threshold (0.5 means -6.02dB, requires RENDER_NORMALIZE&16384)
  RENDER_TRIMEND : trim render end threshold (0.5 means -6.02dB, requires RENDER_NORMALIZE&32768)
  RENDER_DELAY : seconds to delay start of render to allow FX to initialize and load samples (requires RENDER_SETTINGS&(16<<16))
  PROJECT_SRATE : sample rate (ignored unless PROJECT_SRATE_USE set)
  PROJECT_SRATE_USE : set to 1 if project sample rate is used
  PROJECT_TIMEBASE : 0=time, 1=beats position, length, rate, 2=beats position only (read-only)
  PROJECT_TIMEBASE_FLAGS : &1=timebase affects MIDI items, &2=in beats timebase, auto-stretch media items at tempo changes (read-only)
  PROJECT_TCP_UI_FLAGS : &1=pinning tracks to top of arrange view is overridden, &2=hiding tracks in arrange view is overridden
  PLAYBACK_STOP : stop playback at this time position in seconds
  READONLY : set to 1 if project is opened read-only, 0 if normally opened
  DIRTY : set to 1 if project was modified since last save, 0 if unmodified. Can also be set to 256 if project was modified and unsaved, but unmodified since it was last saved to a temporary file (if the appropriate options were configured in prefs).
  ARRANGE_W : arrange view width in pixels (read-only)
  ARRANGE_H : arrange view height in pixels (read-only)
  ARRANGE_MIN_TIMESCALE : arrange view minimum time scale (horizontal zoom), in pixels per second (read-only)
  VKB_NOTECENTER : virtual MIDI keyboard center note
  VKB_LASTVEL : virtual MIDI keyboard last velocity
  VKB_CHANNEL : virtual MIDI keyboard channel
  RULER_HEIGHT : ruler height in pixels
  RULER_LANE_COUNT : number of ruler lanes
  RULER_LANE_ORDER:X : move lane at position X to a new position, -1 to insert a new lane
  RULER_LANE_COLOR:X : ruler lane default color, color&0x1000000 if used
  RULER_LANE_HIDDEN:X : 1 if ruler lane is hidden, 0 otherwise
  RULER_LANE_LOCKED:X : 1 if ruler lane is locked, 0 otherwise
  RULER_LANE_VISIBLE:X : 1 if ruler lane is visible (not hidden and ruler tall enough to display), 0 otherwise (read-only)
  RULER_LANE_DEFAULT:X : 1 if ruler lane is default for new regions, 2 for new markers, 3 for both (OK to set, but returned value is read-only)
  RULER_LANE_TIMEBASE:X : ruler lane default timebase, -1=project default, 0=time, 1=beats (position, length, rate), 2=beats (position only)
  RULER_LANE_FROM_GUID:X : ruler lane number with unique identifier X (see RULER_LANE_GUID)
- **GetSetRepeat** — C: `int GetSetRepeat(int val)`
  -1 == query,0=clear,1=set,>1=toggle . returns new value
- **GetSetRepeatEx** — C: `int GetSetRepeatEx(ReaProject* proj, int val)`
  -1 == query,0=clear,1=set,>1=toggle . returns new value
- **GetSet_LoopTimeRange** — C: `void GetSet_LoopTimeRange(bool isSet, bool isLoop, double* startOut, double* endOut, bool allowautoseek)`
- **GetSet_LoopTimeRange2** — C: `void GetSet_LoopTimeRange2(ReaProject* proj, bool isSet, bool isLoop, double* startOut, double* endOut, bool allowautoseek)`
- **Main_UpdateLoopInfo** — C: `void Main_UpdateLoopInfo(int ignoremask)`
- **GetProjectLength** — C: `double GetProjectLength(ReaProject* proj)`
  returns length of project (maximum of end of media item, markers, end of regions, tempo map
- **GetProjectTimeOffset** — C: `double GetProjectTimeOffset(ReaProject* proj, bool rndframe)`
  Gets project time offset in seconds (project settings - project start time). If rndframe is true, the offset is rounded to a multiple of the project frame size.
- **GetProjectTimeSignature** — C: `void GetProjectTimeSignature(double* bpmOut, double* bpiOut)`
  deprecated
- **GetProjectTimeSignature2** — C: `void GetProjectTimeSignature2(ReaProject* proj, double* bpmOut, double* bpiOut)`
  Gets basic time signature (beats per minute, numerator of time signature in bpi)
  this does not reflect tempo envelopes but is purely what is set in the project settings.
- **SetCurrentBPM** — C: `void SetCurrentBPM(ReaProject* __proj, double bpm, bool wantUndo)`
  set current BPM in project, set wantUndo=true to add undo point
- **Master_GetTempo** — C: `double Master_GetTempo()`
- **Master_GetPlayRate** — C: `double Master_GetPlayRate(ReaProject* project)`
- **Master_GetPlayRateAtTime** — C: `double Master_GetPlayRateAtTime(double time_s, ReaProject* proj)`
- **Master_NormalizeTempo** — C: `double Master_NormalizeTempo(double bpm, bool isnormalized)`
  Convert the tempo to/from a value between 0 and 1, representing bpm in the range of 40-296 bpm.
- **Master_NormalizePlayRate** — C: `double Master_NormalizePlayRate(double playrate, bool isnormalized)`
  Convert play rate to/from a value between 0 and 1, representing the position on the project playrate slider.

## 2. Edit cursor, markers & tempo map

- **GetCursorPosition** — C: `double GetCursorPosition()`
  edit cursor position
- **GetCursorPositionEx** — C: `double GetCursorPositionEx(ReaProject* proj)`
  edit cursor position
- **GetCursorContext** — C: `int GetCursorContext()`
  return the current cursor context: 0 if track panels, 1 if items, 2 if envelopes, otherwise unknown
- **GetCursorContext2** — C: `int GetCursorContext2(bool want_last_valid)`
  0 if track panels, 1 if items, 2 if envelopes, otherwise unknown (unlikely when want_last_valid is true)
- **SetCursorContext** — C: `void SetCursorContext(int mode, TrackEnvelope* envInOptional)`
  You must use this to change the focus programmatically. mode=0 to focus track panels, 1 to focus the arrange window, 2 to focus the arrange window and select env (or env==NULL to clear the current track/take envelope selection)
- **SetEditCurPos** — C: `void SetEditCurPos(double time, bool moveview, bool seekplay)`
- **SetEditCurPos2** — C: `void SetEditCurPos2(ReaProject* proj, double time, bool moveview, bool seekplay)`
- **MoveEditCursor** — C: `void MoveEditCursor(double adjamt, bool dosel)`
- **GoToMarker** — C: `void GoToMarker(ReaProject* proj, int marker_index, bool use_timeline_order)`
  Go to marker. If use_timeline_order==true, marker_index 1 refers to the first marker on the timeline. If use_timeline_order==false, marker_index 1 refers to the first marker with the user-editable index of 1.
- **GoToRegion** — C: `void GoToRegion(ReaProject* proj, int region_index, bool use_timeline_order)`
  Seek to region after current region finishes playing (smooth seek). If use_timeline_order==true, region_index 1 refers to the first region on the timeline. If use_timeline_order==false, region_index 1 refers to the first region with the user-editable index of 1.
- **GetLastMarkerAndCurRegion** — C: `void GetLastMarkerAndCurRegion(ReaProject* proj, double time, int* markeridxOut, int* regionidxOut)`
  Get the last project marker before time, and/or the project region that includes time. markeridx and regionidx are returned not necessarily as the displayed marker/region index, but as the index that can be passed to EnumProjectMarkers. Either or both of markeridx and regionidx may be NULL. See EnumProjectMarkers.
- **CountTempoTimeSigMarkers** — C: `int CountTempoTimeSigMarkers(ReaProject* proj)`
  Count the number of tempo/time signature markers in the project. See GetTempoTimeSigMarker, SetTempoTimeSigMarker, AddTempoTimeSigMarker.
- **GetTempoTimeSigMarker** — C: `bool GetTempoTimeSigMarker(ReaProject* proj, int ptidx, double* timeposOut, int* measureposOut, double* beatposOut, double* bpmOut, int* timesig_numOut, int* timesig_denomOut, bool* lineartempoOut)`
  Get information about a tempo/time signature marker. See CountTempoTimeSigMarkers, SetTempoTimeSigMarker, AddTempoTimeSigMarker.
- **FindTempoTimeSigMarker** — C: `int FindTempoTimeSigMarker(ReaProject* project, double time)`
  Find the tempo/time signature marker that falls at or before this time position (the marker that is in effect as of this time position).
- **AddTempoTimeSigMarker** — C: `bool AddTempoTimeSigMarker(ReaProject* proj, double timepos, double bpm, int timesig_num, int timesig_denom, bool lineartempochange)`
  Deprecated. Use SetTempoTimeSigMarker with ptidx=-1.
- **DeleteTempoTimeSigMarker** — C: `bool DeleteTempoTimeSigMarker(ReaProject* project, int markerindex)`
  Delete a tempo/time signature marker.
- **EditTempoTimeSigMarker** — C: `bool EditTempoTimeSigMarker(ReaProject* project, int markerindex)`
  Open the tempo/time signature marker editor dialog.
- **GetSetTempoTimeSigMarkerFlag** — C: `int GetSetTempoTimeSigMarkerFlag(ReaProject* project, int point_index, int flag, bool is_set)`
  Gets or sets the attribute flag of a tempo/time signature marker. flag &1=sets time signature and starts new measure, &2=does not set tempo, &4=allow previous partial measure if starting new measure, &8=set new metronome pattern if starting new measure, &16=reset ruler grid if starting new measure
- **GetSetTempoTimeSigMarkerBasis** — C: `double GetSetTempoTimeSigMarkerBasis(ReaProject* project, int point_index, double beatbase, bool is_set)`
  Gets or sets the beat basis of a tempo/time signature marker. Supported values are 0.5 (eighth note), 0.75 (dotted eighth), 1.0 (quarter), 1.5 (dotted quarter), 2.0 (half), 3.0 (dotted half).

## 3. Time maps (time <-> QN / measures)

- **TimeMap_timeToQN** — C: `double TimeMap_timeToQN(double tpos)`
  converts project QN position to time.
- **TimeMap_timeToQN_abs** — C: `double TimeMap_timeToQN_abs(ReaProject* proj, double tpos)`
  Converts project time position to quarter note count (QN). QN is counted from the start of the project, regardless of any partial measures. See TimeMap2_timeToQN
- **TimeMap_QNToTime** — C: `double TimeMap_QNToTime(double qn)`
  converts project QN position to time.
- **TimeMap_QNToTime_abs** — C: `double TimeMap_QNToTime_abs(ReaProject* proj, double qn)`
  Converts project quarter note count (QN) to time. QN is counted from the start of the project, regardless of any partial measures. See TimeMap2_QNToTime
- **TimeMap_QNToMeasures** — C: `int TimeMap_QNToMeasures(ReaProject* proj, double qn, double* qnMeasureStartOutOptional, double* qnMeasureEndOutOptional)`
  Find which measure the given QN position falls in.
- **TimeMap_GetMeasureInfo** — C: `double TimeMap_GetMeasureInfo(ReaProject* proj, int measure, double* qn_startOut, double* qn_endOut, int* timesig_numOut, int* timesig_denomOut, double* tempoOut)`
  Get the QN position and time signature information for the start of a measure. Return the time in seconds of the measure start.
- **TimeMap_GetTimeSigAtTime** — C: `void TimeMap_GetTimeSigAtTime(ReaProject* proj, double time, int* timesig_numOut, int* timesig_denomOut, double* tempoOut)`
  get the effective time signature and tempo
- **TimeMap_GetDividedBpmAtTime** — C: `double TimeMap_GetDividedBpmAtTime(double time)`
  get the effective BPM at the time (seconds) position (i.e. 2x in /8 signatures)
- **TimeMap_GetMetronomePattern** — C: `int TimeMap_GetMetronomePattern(ReaProject* proj, double time, char* pattern, int pattern_sz)`
  Fills in a string representing the active metronome pattern. For example, in a 7/8 measure divided 3+4, the pattern might be "ABCABCD". For backwards compatibility, by default the function will return 1 for each primary beat and 2 for each non-primary beat, so "1221222" in this example, and does not support triplets. If buf is set to "EXTENDED", the function will return the full string as displayed in the pattern editor, including all beat types and triplet representations. Pass in "SET:string" with a correctly formed pattern string matching the current time signature numerator to set the click pattern. The time signature numerator can be deduced from the returned string, and the function returns the time signature denominator.
- **TimeMap_curFrameRate** — C: `double TimeMap_curFrameRate(ReaProject* proj, bool* dropFrameOut)`
  Gets project framerate, and optionally whether it is drop-frame timecode
- **TimeMap2_beatsToTime** — C: `double TimeMap2_beatsToTime(ReaProject* proj, double tpos, const int* measuresInOptional)`
  convert a beat position (or optionally a beats+measures if measures is non-NULL) to time.
- **TimeMap2_timeToBeats** — C: `double TimeMap2_timeToBeats(ReaProject* proj, double tpos, int* measuresOutOptional, int* cmlOutOptional, double* fullbeatsOutOptional, int* cdenomOutOptional)`
  convert a time into beats.
  if measures is non-NULL, measures will be set to the measure count, return value will be beats since measure.
  if cml is non-NULL, will be set to current measure length in beats (i.e. time signature numerator)
  if fullbeats is non-NULL, and measures is non-NULL, fullbeats will get the full beat count (same value returned if measures is NULL).
  if cdenom is non-NULL, will be set to the current time signature denominator.
- **TimeMap2_timeToQN** — C: `double TimeMap2_timeToQN(ReaProject* proj, double tpos)`
  converts project time position to QN position.
- **TimeMap2_QNToTime** — C: `double TimeMap2_QNToTime(ReaProject* proj, double qn)`
  converts project QN position to time.
- **TimeMap2_GetNextChangeTime** — C: `double TimeMap2_GetNextChangeTime(ReaProject* proj, double time)`
  when does the next time map (tempo or time sig) change occur
- **TimeMap2_GetDividedBpmAtTime** — C: `double TimeMap2_GetDividedBpmAtTime(ReaProject* proj, double time)`
  get the effective BPM at the time (seconds) position (i.e. 2x in /8 signatures)
- **SnapToGrid** — C: `double SnapToGrid(ReaProject* project, double time_pos)`
- **GetSetProjectGrid** — C: `int GetSetProjectGrid(ReaProject* project, bool set, double* divisionInOutOptional, int* swingmodeInOutOptional, double* swingamtInOutOptional)`
  Get or set the arrange view grid division. 0.25=quarter note, 1.0/3.0=half note triplet, etc. swingmode can be 1 for swing enabled, swingamt is -1..1. swingmode can be 3 for measure-grid. Returns grid configuration flags
- **SetProjectGrid** — C: `void SetProjectGrid(ReaProject* project, double division)`
  Set the arrange view grid division. 0.25=quarter note, 1.0/3.0=half note triplet, etc.

## 4. Actions & commands

- **Main_OnCommand** — C: `void Main_OnCommand(int command, int flag)`
  See Main_OnCommandEx.
- **Main_OnCommandEx** — C: `void Main_OnCommandEx(int command, int flag, ReaProject* proj)`
  Performs an action belonging to the main action section. To perform non-native actions (ReaScripts, custom or extension plugins' actions) safely, see NamedCommandLookup().
- **NamedCommandLookup** — C: `int NamedCommandLookup(const char* command_name)`
  Get the command ID number for named command that was registered by an extension such as "_SWS_ABOUT" or "_113088d11ae641c193a2b7ede3041ad5" for a ReaScript or a custom action.
- **ReverseNamedCommandLookup** — C: `const char* ReverseNamedCommandLookup(int command_id)`
  Get the named command for the given command ID. The returned string will not start with '_' (e.g. it will return "SWS_ABOUT"), it will be NULL if command_id is a native action.
- **GetToggleCommandState** — C: `int GetToggleCommandState(int command_id)`
  See GetToggleCommandStateEx.
- **GetToggleCommandStateEx** — C: `int GetToggleCommandStateEx(int section_id, int command_id)`
  Returns the toggle state of the action. section 0=main, 100=main alt, 32060=MIDI editor, 32061=MIDI event list editor, 32062=MIDI inline editor (toggle feedback not supported), 32063=Media Explorer, 32065=Crossfade Editor. Returns 0=off, 1=on, -1=NA because the action does not have on/off states. For the MIDI editor, the action state for the most recently focused window will be returned.
- **SetToggleCommandState** — C: `bool SetToggleCommandState(int section_id, int command_id, int state)`
  Updates the toggle state of an action, returns true if succeeded. Only ReaScripts can have their toggle states changed programmatically. See RefreshToolbar2.
- **ArmCommand** — C: `void ArmCommand(int cmd, const char* sectionname)`
  arms a command (or disarms if 0 passed) in section sectionname (empty string for main)
- **GetArmedCommand** — C: `int GetArmedCommand(char* secOut, int secOut_sz)`
  gets the currently armed command and section name (returns 0 if nothing armed). section name is empty-string for main section.
- **PromptForAction** — C: `int PromptForAction(int session_mode, int init_id, int section_id)`
  Uses the action list to choose an action. Call with session_mode=1 to create a session (init_id will be the initial action to select, or 0), then poll with session_mode=0, checking return value for user-selected action (will return 0 if no action selected yet, or -1 if the action window is no longer available). When finished, call with session_mode=-1.
- **ShowActionList** — C: `void ShowActionList(KbdSectionInfo* section, HWND callerWnd)`
- **CountActionShortcuts** — C: `int CountActionShortcuts(KbdSectionInfo* section, int cmdID)`
  Returns the number of shortcuts that exist for the given command ID.
  see GetActionShortcutDesc, DeleteActionShortcut, DoActionShortcutDialog.
- **GetActionShortcutDesc** — C: `bool GetActionShortcutDesc(KbdSectionInfo* section, int cmdID, int shortcutidx, char* descOut, int descOut_sz)`
  Get the text description of a specific shortcut for the given command ID.
  See CountActionShortcuts,DeleteActionShortcut,DoActionShortcutDialog.
- **DeleteActionShortcut** — C: `bool DeleteActionShortcut(KbdSectionInfo* section, int cmdID, int shortcutidx)`
  Delete the specific shortcut for the given command ID.
  See CountActionShortcuts, GetActionShortcutDesc, DoActionShortcutDialog.
- **DoActionShortcutDialog** — C: `bool DoActionShortcutDialog(HWND hwnd, KbdSectionInfo* section, int cmdID, int shortcutidx)`
  Open the action shortcut dialog to edit or add a shortcut for the given command ID. If (shortcutidx >= 0 && shortcutidx < CountActionShortcuts()), that specific shortcut will be replaced, otherwise a new shortcut will be added.
  See CountActionShortcuts, GetActionShortcutDesc, DeleteActionShortcut.
- **kbd_enumerateActions** — C: `int kbd_enumerateActions(KbdSectionInfo* section, int idx, const char** nameOut)`
- **kbd_getTextFromCmd** — C: `const char* kbd_getTextFromCmd(int cmd, KbdSectionInfo* section)`
- **Loop_OnArrow** — C: `bool Loop_OnArrow(ReaProject* project, int direction)`
  Move the loop selection left or right. Returns true if snap is enabled.

## 5. Transport buttons & control surface (CSurf)

- **OnPlayButton** — C: `void OnPlayButton()`
  direct way to simulate play button hit
- **OnPlayButtonEx** — C: `void OnPlayButtonEx(ReaProject* proj)`
  direct way to simulate play button hit
- **OnStopButton** — C: `void OnStopButton()`
  direct way to simulate stop button hit
- **OnStopButtonEx** — C: `void OnStopButtonEx(ReaProject* proj)`
  direct way to simulate stop button hit
- **OnPauseButton** — C: `void OnPauseButton()`
  direct way to simulate pause button hit
- **OnPauseButtonEx** — C: `void OnPauseButtonEx(ReaProject* proj)`
  direct way to simulate pause button hit
- **CSurf_FlushUndo** — C: `void CSurf_FlushUndo(bool force)`
  call this to force flushing of the undo states after using CSurf_On*Change()
- **CSurf_GetTouchState** — C: `bool CSurf_GetTouchState(MediaTrack* trackid, int isPan)`
- **CSurf_GoEnd** — C: `void CSurf_GoEnd()`
- **CSurf_GoStart** — C: `void CSurf_GoStart()`
- **CSurf_NumTracks** — C: `int CSurf_NumTracks(bool mcpView)`
- **CSurf_OnArrow** — C: `void CSurf_OnArrow(int whichdir, bool wantzoom)`
- **CSurf_OnFXChange** — C: `bool CSurf_OnFXChange(MediaTrack* trackid, int en)`
- **CSurf_OnFwd** — C: `void CSurf_OnFwd(int seekplay)`
- **CSurf_OnInputMonitorChange** — C: `int CSurf_OnInputMonitorChange(MediaTrack* trackid, int monitor)`
- **CSurf_OnInputMonitorChangeEx** — C: `int CSurf_OnInputMonitorChangeEx(MediaTrack* trackid, int monitor, bool allowgang)`
- **CSurf_OnMuteChange** — C: `bool CSurf_OnMuteChange(MediaTrack* trackid, int mute)`
- **CSurf_OnMuteChangeEx** — C: `bool CSurf_OnMuteChangeEx(MediaTrack* trackid, int mute, bool allowgang)`
- **CSurf_OnPanChange** — C: `double CSurf_OnPanChange(MediaTrack* trackid, double pan, bool relative)`
- **CSurf_OnPanChangeEx** — C: `double CSurf_OnPanChangeEx(MediaTrack* trackid, double pan, bool relative, bool allowGang)`
- **CSurf_OnPause** — C: `void CSurf_OnPause()`
- **CSurf_OnPlay** — C: `void CSurf_OnPlay()`
- **CSurf_OnPlayRateChange** — C: `void CSurf_OnPlayRateChange(double playrate)`
- **CSurf_OnRecArmChange** — C: `bool CSurf_OnRecArmChange(MediaTrack* trackid, int recarm)`
- **CSurf_OnRecArmChangeEx** — C: `bool CSurf_OnRecArmChangeEx(MediaTrack* trackid, int recarm, bool allowgang)`
- **CSurf_OnRecord** — C: `void CSurf_OnRecord()`
- **CSurf_OnRecvPanChange** — C: `double CSurf_OnRecvPanChange(MediaTrack* trackid, int recv_index, double pan, bool relative)`
- **CSurf_OnRecvVolumeChange** — C: `double CSurf_OnRecvVolumeChange(MediaTrack* trackid, int recv_index, double volume, bool relative)`
- **CSurf_OnRew** — C: `void CSurf_OnRew(int seekplay)`
- **CSurf_OnRewFwd** — C: `void CSurf_OnRewFwd(int seekplay, int dir)`
- **CSurf_OnScroll** — C: `void CSurf_OnScroll(int xdir, int ydir)`
- **CSurf_OnSelectedChange** — C: `bool CSurf_OnSelectedChange(MediaTrack* trackid, int selected)`
- **CSurf_OnSendPanChange** — C: `double CSurf_OnSendPanChange(MediaTrack* trackid, int send_index, double pan, bool relative)`
  Add 0x10000000 to the send index in order to access the ordering of the sends/hardware outputs in the mixer, which may be sparsely populated.
- **CSurf_OnSendVolumeChange** — C: `double CSurf_OnSendVolumeChange(MediaTrack* trackid, int send_index, double volume, bool relative)`
  Add 0x10000000 to the send index in order to access the ordering of the sends/hardware outputs in the mixer, which may be sparsely populated.
- **CSurf_OnSoloChange** — C: `bool CSurf_OnSoloChange(MediaTrack* trackid, int solo)`
- **CSurf_OnSoloChangeEx** — C: `bool CSurf_OnSoloChangeEx(MediaTrack* trackid, int solo, bool allowgang)`
- **CSurf_OnStop** — C: `void CSurf_OnStop()`
- **CSurf_OnTempoChange** — C: `void CSurf_OnTempoChange(double bpm)`
- **CSurf_OnTrackSelection** — C: `void CSurf_OnTrackSelection(MediaTrack* trackid)`
- **CSurf_OnVolumeChange** — C: `double CSurf_OnVolumeChange(MediaTrack* trackid, double volume, bool relative)`
- **CSurf_OnVolumeChangeEx** — C: `double CSurf_OnVolumeChangeEx(MediaTrack* trackid, double volume, bool relative, bool allowGang)`
- **CSurf_OnWidthChange** — C: `double CSurf_OnWidthChange(MediaTrack* trackid, double width, bool relative)`
- **CSurf_OnWidthChangeEx** — C: `double CSurf_OnWidthChangeEx(MediaTrack* trackid, double width, bool relative, bool allowGang)`
- **CSurf_OnZoom** — C: `void CSurf_OnZoom(int xdir, int ydir)`
- **CSurf_ResetAllCachedVolPanStates** — C: `void CSurf_ResetAllCachedVolPanStates()`
- **CSurf_ScrubAmt** — C: `void CSurf_ScrubAmt(double amt)`
- **CSurf_SetAutoMode** — C: `void CSurf_SetAutoMode(int mode, IReaperControlSurface* ignoresurf)`
- **CSurf_SetPlayState** — C: `void CSurf_SetPlayState(bool play, bool pause, bool rec, IReaperControlSurface* ignoresurf)`
- **CSurf_SetRepeatState** — C: `void CSurf_SetRepeatState(bool rep, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfaceMute** — C: `void CSurf_SetSurfaceMute(MediaTrack* trackid, bool mute, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfacePan** — C: `void CSurf_SetSurfacePan(MediaTrack* trackid, double pan, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfaceRecArm** — C: `void CSurf_SetSurfaceRecArm(MediaTrack* trackid, bool recarm, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfaceSelected** — C: `void CSurf_SetSurfaceSelected(MediaTrack* trackid, bool selected, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfaceSolo** — C: `void CSurf_SetSurfaceSolo(MediaTrack* trackid, bool solo, IReaperControlSurface* ignoresurf)`
- **CSurf_SetSurfaceVolume** — C: `void CSurf_SetSurfaceVolume(MediaTrack* trackid, double volume, IReaperControlSurface* ignoresurf)`
- **CSurf_SetTrackListChange** — C: `void CSurf_SetTrackListChange()`
- **CSurf_TrackFromID** — C: `MediaTrack* CSurf_TrackFromID(int idx, bool mcpView)`
- **CSurf_TrackToID** — C: `int CSurf_TrackToID(MediaTrack* track, bool mcpView)`

## 6. View, zoom & scroll

- **adjustZoom** — C: `void adjustZoom(double amt, int forceset, bool doupd, int centermode)`
  forceset=0,doupd=true,centermode=-1 for default
- **GetHZoomLevel** — C: `double GetHZoomLevel()`
  returns pixels/second
- **GetSet_ArrangeView2** — C: `void GetSet_ArrangeView2(ReaProject* proj, bool isSet, int screen_x_start, int screen_x_end, double* start_timeInOut, double* end_timeInOut)`
  Gets or sets the arrange view start/end time for screen coordinates. use screen_x_start=screen_x_end=0 to use the full arrange view's start/end time
- **UpdateArrange** — C: `void UpdateArrange()`
  Redraw the arrange view
- **GetMousePosition** — C: `void GetMousePosition(int* xOut, int* yOut)`
  get mouse position in screen coordinates
- **GetMouseModifier** — C: `void GetMouseModifier(const char* context, int modifier_flag, char* actionOut, int actionOut_sz)`
  Get the current mouse modifier assignment for a specific modifier key assignment, in a specific context.
  action will be filled in with the command ID number for a built-in mouse modifier
  or built-in REAPER command ID, or the custom action ID string.
  Note: the action string may have a space and 'c' or 'm' appended to it to specify command ID vs mouse modifier ID.
  You can add the 1024 bit to the flag, which will allow you to retrieve the no-move or no-select bits of the assignment, if the assignment is an action ID.
  See SetMouseModifier for more information.
- **SetMouseModifier** — C: `void SetMouseModifier(const char* context, int modifier_flag, const char* action)`
  Set the mouse modifier assignment for a specific modifier key assignment, in a specific context.
  Context is a string like "MM_CTX_ITEM" (see reaper-mouse.ini) or "Media item left drag" (unlocalized).
  Modifier flag is a number from 0 to 15: add 1 for shift, 2 for control, 4 for alt, 8 for win.
  (macOS: add 1 for shift, 2 for command, 4 for opt, 8 for control.)
  For left-click and double-click contexts, the action can be any built-in command ID number
  or any custom action ID string. Find built-in command IDs in the REAPER actions window
  (enable "show command IDs" in the context menu), and find custom action ID strings in reaper-kb.ini.
  The action string may be a mouse modifier ID (see reaper-mouse.ini) with " m" appended to it,
  or (for click/double-click contexts) a command ID with " c" appended to it,
  or the text that appears in the mouse modifiers preferences dialog, like "Move item" (unlocalized).
  For example, SetMouseModifier("MM_CTX_ITEM", 0, "1 m") and SetMouseModifier("Media item left drag", 0, "Move item") are equivalent.
  SetMouseModifier(context, modifier_flag, -1) will reset that mouse modifier to default.
  SetMouseModifier(context, -1, -1) will reset the entire context to default.
  SetMouseModifier(-1, -1, -1) will reset all contexts to default.
  You can add the 1024 bit to flag, and use this to set the no-move (1) and/or no-select (2) values for an action ID, after the command modifier was set.
  See GetMouseModifier.
- **my_getViewport** — C: `void my_getViewport(RECT* r, const RECT* sr, bool wantWorkArea)`
- **GetMixerScroll** — C: `MediaTrack* GetMixerScroll()`
  Get the leftmost track visible in the mixer
- **SetMixerScroll** — C: `MediaTrack* SetMixerScroll(MediaTrack* leftmosttrack)`
  Scroll the mixer so that leftmosttrack is the leftmost visible track. Returns the leftmost track after scrolling, which may be different from the passed-in track if there are not enough tracks to its right.
- **ViewPrefs** — C: `void ViewPrefs(int page, const char* pageByName)`
  Opens the prefs to a page, use pageByName if page is 0.
  ReaScript/EEL2 Built-in Function List

## 7. Project, markers / regions & extension state

- **CountProjectMarkers** — C: `int CountProjectMarkers(ReaProject* proj, int* num_markersOut, int* num_regionsOut)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker. num_markersOut and num_regionsOut may be NULL.
- **EnumProjectMarkers** — C: `int EnumProjectMarkers(int idx, bool* isrgnOut, double* posOut, double* rgnendOut, const char** nameOut, int* markrgnindexnumberOut)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker
- **EnumProjectMarkers2** — C: `int EnumProjectMarkers2(ReaProject* proj, int idx, bool* isrgnOut, double* posOut, double* rgnendOut, const char** nameOut, int* markrgnindexnumberOut)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker
- **EnumProjectMarkers3** — C: `int EnumProjectMarkers3(ReaProject* proj, int idx, bool* isrgnOut, double* posOut, double* rgnendOut, const char** nameOut, int* markrgnindexnumberOut, int* colorOut)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker
- **AddProjectMarker** — C: `int AddProjectMarker(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx)`
  Returns the index of the created marker/region, or -1 on failure. Supply wantidx>=0 if you want a particular index number, but you'll get a different index number if wantidx is already in use.discouraged. see AddRegionOrMarker, GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Note: this function can't clear a marker's name (an empty string will leave the name unchanged), see SetProjectMarker4.
- **AddProjectMarker2** — C: `int AddProjectMarker2(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx, int color)`
  Returns the index of the created marker/region, or -1 on failure. Supply wantidx>=0 if you want a particular index number, but you'll get a different index number if wantidx is already in use. color should be 0 (default color), or ColorToNative(r,g,b)|0x1000000. discouraged. see AddRegionOrMarker, GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Note: this function can't clear a marker's name (an empty string will leave the name unchanged), see SetProjectMarker4.
- **SetProjectMarker** — C: `bool SetProjectMarker(int markrgnindexnumber, bool isrgn, double pos, double rgnend, const char* name)`
  discouraged. see AddRegionOrMarker, GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Note: this function can't clear a marker's name (an empty string will leave the name unchanged), see SetProjectMarker4.
- **SetProjectMarker2** — C: `bool SetProjectMarker2(ReaProject* proj, int markrgnindexnumber, bool isrgn, double pos, double rgnend, const char* name)`
  discouraged. see AddRegionOrMarker, GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Note: this function can't clear a marker's name (an empty string will leave the name unchanged), see SetProjectMarker4.
- **SetProjectMarker3** — C: `bool SetProjectMarker3(ReaProject* proj, int markrgnindexnumber, bool isrgn, double pos, double rgnend, const char* name, int color)`
  discouraged. see AddRegionOrMarker, GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Note: this function can't clear a marker's name (an empty string will leave the name unchanged), see SetProjectMarker4.
- **SetProjectMarker4** — C: `bool SetProjectMarker4(ReaProject* proj, int markrgnindexnumber, bool isrgn, double pos, double rgnend, const char* name, int color, int flags)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. color should be 0 to not change, or ColorToNative(r,g,b)|0x1000000, flags&1 to clear name
- **SetProjectMarkerByIndex** — C: `bool SetProjectMarkerByIndex(ReaProject* proj, int markrgnidx, bool isrgn, double pos, double rgnend, int IDnumber, const char* name, int color)`
  discouraged. See SetProjectMarkerByIndex2.
- **SetProjectMarkerByIndex2** — C: `bool SetProjectMarkerByIndex2(ReaProject* proj, int markrgnidx, bool isrgn, double pos, double rgnend, int IDnumber, const char* name, int color, int flags)`
  discouraged. see GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String. Differs from SetProjectMarker4 in that markrgnidx is 0 for the first marker/region, 1 for the next, etc (see EnumProjectMarkers3), rather than representing the displayed marker/region ID number (see SetProjectMarker3). IDnumber < 0 to ignore. Function will fail if attempting to set a duplicate ID number for a region (duplicate ID numbers for markers are OK). flags&1 to clear name. If flags&2, markers will not be re-sorted, and after making updates, you MUST call SetProjectMarkerByIndex2 with markrgnidx=-1 and flags&2 to force re-sort/UI updates.
- **DeleteProjectMarker** — C: `bool DeleteProjectMarker(ReaProject* proj, int markrgnindexnumber, bool isrgn)`
  Delete a marker. proj==NULL for the active project.
- **DeleteProjectMarkerByIndex** — C: `bool DeleteProjectMarkerByIndex(ReaProject* proj, int markrgnidx)`
  Differs from DeleteProjectMarker only in that markrgnidx is 0 for the first marker/region, 1 for the next, etc (see EnumProjectMarkers3), rather than representing the displayed marker/region ID number (see SetProjectMarker4).
- **GetRegionOrMarker** — C: `ProjectMarker* GetRegionOrMarker(ReaProject* proj, int index, const char* guidStr)`
  get a single region or marker by internal index, or if index < 0, by GUID. See GetNumRegionsOrMarkers, GetRegionOrMarkerInfo_Value, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String
- **GetRegionOrMarkerInfo_Value** — C: `double GetRegionOrMarkerInfo_Value(ReaProject* proj, ProjectMarker* regionOrMarker, const char* parameterName)`
  "D_STARTPOS", "D_ENDPOS" (= D_STARTPOS for markers), "I_INDEX" (internal index), "I_NUMBER" (displayed index number), "I_LANENUMBER" (can be set, but returned value is read-only), "I_CUSTOMCOLOR", "I_DISPLAYEDCOLOR", "B_ISREGION", "B_UISEL", "B_HIDDEN", "B_VISIBLE" (marker and lane visible, read-only). See GetNumRegionsOrMarkers, GetRegionOrMarker, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String
- **SetRegionOrMarkerInfo_Value** — C: `double SetRegionOrMarkerInfo_Value(ReaProject* proj, ProjectMarker* regionOrMarker, const char* parameterName, double setNewValue)`
  "D_STARTPOS", "D_ENDPOS" (= D_STARTPOS for markers), "I_NUMBER" (displayed index number), "I_LANENUMBER" (can be set, but returned value is read-only, "I_CUSTOMCOLOR", "B_UISEL", "B_HIDDEN". See GetNumRegionsOrMarkers, GetRegionOrMarker, GetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String
- **GetSetRegionOrMarkerInfo_String** — C: `bool GetSetRegionOrMarkerInfo_String(ReaProject* proj, ProjectMarker* regionOrMarker, const char* parameterName, char* stringNeedBig, bool setNewValue)`
  "GUID" (read-only), "P_NAME". See GetNumRegionsOrMarkers, GetRegionOrMarker, GetRegionOrMarkerInfo_Value, SetRegionOrMarkerInfo_Value
- **AddRegionOrMarker** — C: `ProjectMarker* AddRegionOrMarker(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx, int color)`
  Supply wantidx>=0 if you want a particular index number, but you'll get a different index number if wantidx is already in use. color should be 0 (default color), or ColorToNative(r,g,b)|0x1000000
- **GetNumRegionsOrMarkers** — C: `int GetNumRegionsOrMarkers(ReaProject* proj)`
  the total number of regions and markers in the project. See GetRegionOrMarker, GetRegionOrMarkerInfo_Value, SetRegionOrMarkerInfo_Value, GetSetRegionOrMarkerInfo_String
- **GetProjectName** — C: `void GetProjectName(ReaProject* proj, char* bufOut, int bufOut_sz)`
- **GetProjectPath** — C: `void GetProjectPath(char* bufOut, int bufOut_sz)`
  Get the project recording path.
- **GetProjectPathEx** — C: `void GetProjectPathEx(ReaProject* proj, char* bufOut, int bufOut_sz)`
  Get the project recording path.
- **GetProjectStateChangeCount** — C: `int GetProjectStateChangeCount(ReaProject* proj)`
  returns an integer that changes when the project state changes
- **GetSetProjectInfo_String** — C: `bool GetSetProjectInfo_String(ReaProject* project, const char* desc, char* valuestrNeedBig, bool is_set)`
  Get or set project information.
  PROJECT_NAME : project file name (read-only, is_set will be ignored)
  PROJECT_TITLE : title field from Project Settings/Notes dialog
  PROJECT_AUTHOR : author field from Project Settings/Notes dialog
  TRACK_GROUP_NAME:X : track group name, X should be 1..64
  RULER_LANE_NAME:X : ruler lane name
  RULER_LANE_GUID:X : ruler lane unique identifier
  MARKER_GUID:X : discouraged. see GetRegionOrMarker, GetSetRegionOrMarkerInfo_String
  MARKER_INDEX_FROM_GUID:{GUID} : discouraged. see GetRegionOrMarker, GetSetRegionOrMarkerInfo_String
  OPENCOPY_CFGIDX : integer for the configuration of format to use when creating copies/applying FX. 0=wave (auto-depth), 1=APPLYFX_FORMAT, 2=RECORD_FORMAT
  RECORD_PATH : recording directory -- may be blank or a relative path, to get the effective path see GetProjectPathEx()
  RECORD_PATH_SECONDARY : secondary recording directory
  RECORD_FORMAT : base64-encoded sink configuration (see project files, etc). Callers can also pass a simple 4-byte string (non-base64-encoded), e.g. "evaw" or "l3pm", to use default settings for that sink type.
  APPLYFX_FORMAT : base64-encoded sink configuration (see project files, etc). Used only if RECFMT_OPENCOPY is set to 1. Callers can also pass a simple 4-byte string (non-base64-encoded), e.g. "evaw" or "l3pm", to use default settings for that sink type.
  RECTAG : project recording tag wildcard ($rectag). Can be used in Preferences/Audio/Recording to auto-name recorded files.
  RENDER_FILE : render directory
  RENDER_PATTERN : render file name (may contain wildcards)
  RENDER_EXTRAFILEDIR : alternate path for renderedfile.wav.rpp and render_stats.html
  RENDER_METADATA : get or set the metadata saved with the project (not metadata embedded in project media). Example, ID3 album name metadata: valuestr="ID3:TALB" to get, valuestr="ID3:TALB|my album name" to set. Call with valuestr="" and is_set=false to get a semicolon-separated list of defined project metadata identifiers.
  RENDER_TARGETS : semicolon separated list of files that would be written if the project is rendered using the most recent render settings
  RENDER_STATS : (read-only) semicolon separated list of statistics for the most recently rendered files. call with valuestr="XXX" to run an action (for example, "42437"=dry run render selected items) before returning statistics.
  RENDER_STATS_SUMMARY : (read-only) human-readable summary of statistics for the most recently rendered files. call with valuestr="XXX" to run an action (for example, "42437"=dry run render selected items) before returning statistics.
  RENDER_FORMAT : base64-encoded sink configuration (see project files, etc). Callers can also pass a simple 4-byte string (non-base64-encoded), e.g. "evaw" or "l3pm", to use default settings for that sink type.
  RENDER_FORMAT2 : base64-encoded secondary sink configuration. Callers can also pass a simple 4-byte string (non-base64-encoded), e.g. "evaw" or "l3pm", to use default settings for that sink type, or "" to disable secondary render.
  Formats available on this machine:
  "wave" "aiff" "caff" "flac" "mp3l" "OggS" "oggv" "wvpk" "raw " "iso " "ddp " "FFMP" "WMF " "GIF " "LCF "
- **GetSetProjectAuthor** — C: `void GetSetProjectAuthor(ReaProject* proj, bool set, char* author, int author_sz)`
  deprecated, see GetSetProjectInfo_String with desc="PROJECT_AUTHOR"
- **GetSetProjectNotes** — C: `void GetSetProjectNotes(ReaProject* proj, bool set, char* notesNeedBig, int notesNeedBig_sz)`
  gets or sets project notes, notesNeedBig_sz is ignored when setting
- **MarkProjectDirty** — C: `void MarkProjectDirty(ReaProject* proj)`
  Marks project as dirty (needing save) if 'undo/prompt to save' is enabled in preferences.
- **IsProjectDirty** — C: `int IsProjectDirty(ReaProject* proj)`
  Is the project dirty (needing save)? Always returns 0 if 'undo/prompt to save' is disabled in preferences.
- **Main_SaveProject** — C: `void Main_SaveProject(ReaProject* proj, bool forceSaveAsInOptional)`
  Save the project.
- **Main_SaveProjectEx** — C: `void Main_SaveProjectEx(ReaProject* proj, const char* filename, int options)`
  Save the project. options: &1=save selected tracks as track template, &2=include media with track template, &4=include envelopes with track template, &8=if not saving template, set as the new project filename for this ReaProject. See Main_openProject, Main_SaveProject.
- **Main_openProject** — C: `void Main_openProject(const char* name)`
  opens a project. will prompt the user to save unless name is prefixed with 'noprompt:'. If name is prefixed with 'template:', project file will be loaded as a template.
  If name is prefixed with 'fxoffline:', it will be loaded with all FX offline. If passed a .RTrackTemplate file, adds the template to the existing project.
- **SelectProjectInstance** — C: `void SelectProjectInstance(ReaProject* proj)`
- **EnumProjects** — C: `ReaProject* EnumProjects(int idx, char* projfnOutOptional, int projfnOutOptional_sz)`
  idx=-1 for current project,projfn can be NULL if not interested in filename. use idx 0x40000000 for currently rendering project, if any.
- **GetExtState** — C: `const char* GetExtState(const char* section, const char* key)`
  Get the extended state value for a specific section and key. See SetExtState, DeleteExtState, HasExtState.
- **SetExtState** — C: `void SetExtState(const char* section, const char* key, const char* value, bool persist)`
  Set the extended state value for a specific section and key. persist=true means the value should be stored and reloaded the next time REAPER is opened. Note that with persist=true set, the value will be encoded as a text line in an .ini file and may behaved in unexpected ways if it contains any newlines: do not pass a string with newlines to this function. To save arbitrary data persistently, use base64 encoding or similar. See GetExtState, DeleteExtState, HasExtState.
- **HasExtState** — C: `bool HasExtState(const char* section, const char* key)`
  Returns true if there exists an extended state value for a specific section and key. See SetExtState, GetExtState, DeleteExtState.
- **DeleteExtState** — C: `void DeleteExtState(const char* section, const char* key, bool persist)`
  Delete the extended state value for a specific section and key. persist=true means the value should remain deleted the next time REAPER is opened. See SetExtState, GetExtState, HasExtState.
- **EnumProjExtState** — C: `bool EnumProjExtState(ReaProject* proj, const char* extname, int idx, char* keyOutOptional, int keyOutOptional_sz, char* valOutOptional, int valOutOptional_sz)`
  Enumerate the data stored with the project for a specific extname. Returns false when there is no more data. See SetProjExtState, GetProjExtState.
- **GetProjExtState** — C: `int GetProjExtState(ReaProject* proj, const char* extname, const char* key, char* valOutNeedBig, int valOutNeedBig_sz)`
  Get the value previously associated with this extname and key, the last time the project was saved. See SetProjExtState, EnumProjExtState.
- **SetProjExtState** — C: `int SetProjExtState(ReaProject* proj, const char* extname, const char* key, const char* value)`
  Save a key/value pair for a specific extension, to be restored the next time this specific project is loaded. Typically extname will be the name of a reascript or extension section. If key is NULL or "", all extended data for that extname will be deleted. If val is NULL or "", the data previously associated with that key will be deleted. Returns the size of the state for this extname. See GetProjExtState, EnumProjExtState.

## 8. Tracks, selection & media items

- **CountTracks** — C: `int CountTracks(ReaProject* projOptional)`
  count the number of tracks in the project (proj=0 for active project)
- **GetTrack** — C: `MediaTrack* GetTrack(ReaProject* proj, int trackidx)`
  get a track from a project by track count (zero-based) (proj=0 for active project)
- **GetTrackName** — C: `bool GetTrackName(MediaTrack* track, char* bufOut, int bufOut_sz)`
  Returns "MASTER" for master track, "Track N" if track has no name.
- **GetSelectedTrack** — C: `MediaTrack* GetSelectedTrack(ReaProject* proj, int seltrackidx)`
  Get a selected track from a project (proj=0 for active project) by selected track count (zero-based). This function ignores the master track, see GetSelectedTrack2.
- **GetSelectedTrack2** — C: `MediaTrack* GetSelectedTrack2(ReaProject* proj, int seltrackidx, bool wantmaster)`
  Get a selected track from a project (proj=0 for active project) by selected track count (zero-based).
- **SetTrackSelected** — C: `void SetTrackSelected(MediaTrack* track, bool selected)`
- **SetOnlyTrackSelected** — C: `void SetOnlyTrackSelected(MediaTrack* track)`
  Set exactly one track selected, deselect all others
- **CountSelectedTracks** — C: `int CountSelectedTracks(ReaProject* proj)`
  Count the number of selected tracks in the project (proj=0 for active project). This function ignores the master track, see CountSelectedTracks2.
- **CountSelectedTracks2** — C: `int CountSelectedTracks2(ReaProject* proj, bool wantmaster)`
  Count the number of selected tracks in the project (proj=0 for active project).
- **GetMasterTrack** — C: `MediaTrack* GetMasterTrack(ReaProject* proj)`
- **CountMediaItems** — C: `int CountMediaItems(ReaProject* proj)`
  count the number of items in the project (proj=0 for active project)
- **CountSelectedMediaItems** — C: `int CountSelectedMediaItems(ReaProject* proj)`
  Discouraged, because GetSelectedMediaItem can be inefficient if media items are added, rearranged, or deleted in between calls. Instead see CountMediaItems, GetMediaItem, IsMediaItemSelected.
- **GetSelectedMediaItem** — C: `MediaItem* GetSelectedMediaItem(ReaProject* proj, int selitem)`
  Discouraged, because GetSelectedMediaItem can be inefficient if media items are added, rearranged, or deleted in between calls. Instead see CountMediaItems, GetMediaItem, IsMediaItemSelected.
- **GetMediaItemInfo_Value** — C: `double GetMediaItemInfo_Value(MediaItem* item, const char* parmname)`
  Get media item numerical-value attributes.
  B_MUTE : bool * : muted (item solo overrides). setting this value will clear C_MUTE_SOLO.
  B_MUTE_ACTUAL : bool * : muted (ignores solo). setting this value will not affect C_MUTE_SOLO.
  C_LANEPLAYS : char * : on fixed lane tracks, 0=this item lane does not play, 1=this item lane plays exclusively, 2=this item lane plays and other lanes also play, -1=this item is on a non-visible, non-playing lane on a formerly fixed-lane track (read-only)
  C_MUTE_SOLO : char * : solo override (-1=soloed, 0=no override, 1=unsoloed). note that this API does not automatically unsolo other items when soloing (nor clear the unsolos when clearing the last soloed item), it must be done by the caller via action or via this API.
  B_LOOPSRC : bool * : loop source
  B_ALLTAKESPLAY : bool * : all takes play
  B_UISEL : bool * : selected in arrange view
  C_BEATATTACHMODE : char * : item timebase, -1=track or project default, 1=beats (position, length, rate), 2=beats (position only). for auto-stretch timebase: C_BEATATTACHMODE=1, C_AUTOSTRETCH=1
  C_AUTOSTRETCH: : char * : auto-stretch at project tempo changes, 1=enabled, requires C_BEATATTACHMODE=1
  C_LOCK : char * : locked, &1=locked
  D_VOL : double * : item volume, 0=-inf, 0.5=-6dB, 1=+0dB, 2=+6dB, etc
  D_POSITION : double * : item position in seconds
  D_LENGTH : double * : item length in seconds
  D_SNAPOFFSET : double * : item snap offset in seconds
  D_FADEINLEN : double * : item manual fadein length in seconds
  D_FADEOUTLEN : double * : item manual fadeout length in seconds
  D_FADEINDIR : double * : item fadein curvature, -1..1 (v7.80 and earlier)
  D_FADEOUTDIR : double * : item fadeout curvature, -1..1 (v7.80 and earlier)
  D_FADEINDIR_NEW : double * : item fadein curvature, -1..1 (v7.81 and later)
  D_FADEOUTDIR_NEW : double * : item fadeout curvature, -1..1 (v7.81 and later)
  D_FADEINDIR2_NEW : double * : item fadein S parameter, -1..1 (v7.81 and later)
  D_FADEOUTDIR2_NEW : double * : item fadeout S parameter, -1..1 (v7.81 and later)
  D_FADEINLEN_AUTO : double * : item auto-fadein length in seconds, 0 means no auto-fadein
  D_FADEOUTLEN_AUTO : double * : item auto-fadeout length in seconds, 0 means no auto-fadeout
  C_FADEINSHAPE : int * : fadein shape, 0..6, 0=linear (v7.80 and earlier, FADEINDIR_NEW/FADEINDIR2_NEW determine shape in v7.81 and later)
  C_FADEOUTSHAPE : int * : fadeout shape, 0..6, 0=linear (v7.80 and earlier, FADEINDIR_NEW/FADEINDIR2_NEW determine shape in v7.81 and later)
  I_FADELPF : int * : low pass frequency fade, &1=fade-in, &2=fade-out
  I_GROUPID : int * : group ID, 0=no group
  I_MIXFLAG : int * : mix flag, -1=project default, 0=enclosed items replace enclosing items, 1=items always mix, 2=later items always replace earlier items
  I_LASTY : int * : Y-position (relative to top of track) in pixels (read-only)
  I_LASTH : int * : height in pixels (read-only)
  I_CUSTOMCOLOR : int * : custom color, OS dependent color|0x1000000 (i.e. ColorToNative(r,g,b)|0x1000000). If you do not |0x1000000, then it will not be used, but will store the color
  I_CURTAKE : int * : active take number
  IP_ITEMNUMBER : int : item number on this track (read-only, returns the item number directly)
  F_FREEMODE_Y : float * : free item positioning or fixed lane Y-position. 0=top of track, 1.0=bottom of track
  F_FREEMODE_H : float * : free item positioning or fixed lane height. 0.5=half the track height, 1.0=full track height
  I_FIXEDLANE : int * : fixed lane of item (fine to call with setNewValue, but returned value is read-only)
  B_FIXEDLANE_HIDDEN : bool * : true if displaying only one fixed lane and this item is in a different lane (read-only)
  P_TRACK : MediaTrack * : (read-only)
- **SetMediaItemInfo_Value** — C: `bool SetMediaItemInfo_Value(MediaItem* item, const char* parmname, double newvalue)`
  Set media item numerical-value attributes.
  B_MUTE : bool * : muted (item solo overrides). setting this value will clear C_MUTE_SOLO.
  B_MUTE_ACTUAL : bool * : muted (ignores solo). setting this value will not affect C_MUTE_SOLO.
  C_LANEPLAYS : char * : on fixed lane tracks, 0=this item lane does not play, 1=this item lane plays exclusively, 2=this item lane plays and other lanes also play, -1=this item is on a non-visible, non-playing lane on a formerly fixed-lane track (read-only)
  C_MUTE_SOLO : char * : solo override (-1=soloed, 0=no override, 1=unsoloed). note that this API does not automatically unsolo other items when soloing (nor clear the unsolos when clearing the last soloed item), it must be done by the caller via action or via this API.
  B_LOOPSRC : bool * : loop source
  B_ALLTAKESPLAY : bool * : all takes play
  B_UISEL : bool * : selected in arrange view
  C_BEATATTACHMODE : char * : item timebase, -1=track or project default, 1=beats (position, length, rate), 2=beats (position only). for auto-stretch timebase: C_BEATATTACHMODE=1, C_AUTOSTRETCH=1
  C_AUTOSTRETCH: : char * : auto-stretch at project tempo changes, 1=enabled, requires C_BEATATTACHMODE=1
  C_LOCK : char * : locked, &1=locked
  D_VOL : double * : item volume, 0=-inf, 0.5=-6dB, 1=+0dB, 2=+6dB, etc
  D_POSITION : double * : item position in seconds
  D_LENGTH : double * : item length in seconds
  D_SNAPOFFSET : double * : item snap offset in seconds
  D_FADEINLEN : double * : item manual fadein length in seconds
  D_FADEOUTLEN : double * : item manual fadeout length in seconds
  D_FADEINDIR : double * : item fadein curvature, -1..1 (v7.80 and earlier)
  D_FADEOUTDIR : double * : item fadeout curvature, -1..1 (v7.80 and earlier)
  D_FADEINDIR_NEW : double * : item fadein curvature, -1..1 (v7.81 and later)
  D_FADEOUTDIR_NEW : double * : item fadeout curvature, -1..1 (v7.81 and later)
  D_FADEINDIR2_NEW : double * : item fadein S parameter, -1..1 (v7.81 and later)
  D_FADEOUTDIR2_NEW : double * : item fadeout S parameter, -1..1 (v7.81 and later)
  D_FADEINLEN_AUTO : double * : item auto-fadein length in seconds, 0 means no auto-fadein
  D_FADEOUTLEN_AUTO : double * : item auto-fadeout length in seconds, 0 means no auto-fadeout
  C_FADEINSHAPE : int * : fadein shape, 0..6, 0=linear (v7.80 and earlier, FADEINDIR_NEW/FADEINDIR2_NEW determine shape in v7.81 and later)
  C_FADEOUTSHAPE : int * : fadeout shape, 0..6, 0=linear (v7.80 and earlier, FADEINDIR_NEW/FADEINDIR2_NEW determine shape in v7.81 and later)
  I_FADELPF : int * : low pass frequency fade, &1=fade-in, &2=fade-out
  I_GROUPID : int * : group ID, 0=no group
  I_MIXFLAG : int * : mix flag, -1=project default, 0=enclosed items replace enclosing items, 1=items always mix, 2=later items always replace earlier items
  I_LASTY : int * : Y-position (relative to top of track) in pixels (read-only)
  I_LASTH : int * : height in pixels (read-only)
  I_CUSTOMCOLOR : int * : custom color, OS dependent color|0x1000000 (i.e. ColorToNative(r,g,b)|0x1000000). If you do not |0x1000000, then it will not be used, but will store the color
  I_CURTAKE : int * : active take number
  IP_ITEMNUMBER : int : item number on this track (read-only, returns the item number directly)
  F_FREEMODE_Y : float * : free item positioning or fixed lane Y-position. 0=top of track, 1.0=bottom of track
  F_FREEMODE_H : float * : free item positioning or fixed lane height. 0.5=half the track height, 1.0=full track height
  I_FIXEDLANE : int * : fixed lane of item (fine to call with setNewValue, but returned value is read-only)
  B_FIXEDLANE_HIDDEN : bool * : true if displaying only one fixed lane and this item is in a different lane (read-only)
- **GetSetMediaItemInfo_String** — C: `bool GetSetMediaItemInfo_String(MediaItem* item, const char* parmname, char* stringNeedBig, bool setNewValue)`
  Gets/sets an item attribute string:
  P_NOTES : char * : item note text (do not write to returned pointer, use setNewValue to update)
  P_EXT:xyz : char * : extension-specific persistent data
  GUID : GUID * : 16-byte GUID, can query or update. If using a _String() function, GUID is a string {xyz-...}.
- **SetMediaItemSelected** — C: `void SetMediaItemSelected(MediaItem* item, bool selected)`
- **IsMediaItemSelected** — C: `bool IsMediaItemSelected(MediaItem* item)`
- **SplitMediaItem** — C: `MediaItem* SplitMediaItem(MediaItem* item, double position)`
  the original item becomes the left-hand split, the function returns the right-hand split (or NULL if the split failed)
- **GetMediaItem_Track** — C: `MediaTrack* GetMediaItem_Track(MediaItem* item)`
  Get parent track of media item
- **GetTrackMediaItem** — C: `MediaItem* GetTrackMediaItem(MediaTrack* tr, int itemidx)`
- **GetTrackNumMediaItems** — C: `int GetTrackNumMediaItems(MediaTrack* tr)`
- **InsertMedia** — C: `int InsertMedia(const char* file, int mode)`
  mode: 0=add to current track, 1=add new track, 3=add to selected items as takes, &4=stretch/loop to fit time sel, &8=try to match tempo 1x, &16=try to match tempo 0.5x, &32=try to match tempo 2x, &64=don't preserve pitch when matching tempo, &128=no loop/section if startpct/endpct set, &256=force loop regardless of global preference for looping imported items, &512=use high word as absolute track index if mode&3==0 or mode&2048, &1024=insert into reasamplomatic on a new track (add 1 to insert on last selected track), &2048=insert into open reasamplomatic instance (add 512 to use high word as absolute track index), &4096=move to source preferred position (BWF timecode start offset), &8192=reverse. &16384=apply ripple according to project setting
- **InsertMediaSection** — C: `int InsertMediaSection(const char* file, int mode, double startpct, double endpct, double pitchshift)`
  See InsertMedia.
- **GetMediaSourceLength** — C: `double GetMediaSourceLength(PCM_source* source, bool* lengthIsQNOut)`
  Returns the length of the source media. If the media source is beat-based, the length will be in quarter notes, otherwise it will be in seconds.

## 9. Undo

- **Undo_BeginBlock** — C: `void Undo_BeginBlock()`
  call to start a new block
- **Undo_BeginBlock2** — C: `void Undo_BeginBlock2(ReaProject* proj)`
  call to start a new block
- **Undo_EndBlock** — C: `void Undo_EndBlock(const char* descchange, int extraflags)`
  call to end the block,with extra flags if any,and a description
- **Undo_EndBlock2** — C: `void Undo_EndBlock2(ReaProject* proj, const char* descchange, int extraflags)`
  call to end the block,with extra flags if any,and a description
- **Undo_CanUndo2** — C: `const char* Undo_CanUndo2(ReaProject* proj)`
  returns string of last action,if able,NULL if not
- **Undo_CanRedo2** — C: `const char* Undo_CanRedo2(ReaProject* proj)`
  returns string of next action,if able,NULL if not
- **Undo_DoUndo2** — C: `int Undo_DoUndo2(ReaProject* proj)`
  nonzero if success
- **Undo_DoRedo2** — C: `int Undo_DoRedo2(ReaProject* proj)`
  nonzero if success
- **Undo_GetNumEntries** — C: `int Undo_GetNumEntries(ReaProject* proj)`
  Gets number of undo entries
- **Undo_GetCurEntry** — C: `int Undo_GetCurEntry(ReaProject* proj)`
  Gets current undo entry index
- **Undo_SetCurPos** — C: `void Undo_SetCurPos(ReaProject* proj, int index, int loadAltTreeOptional)`
  Loads an undo entry. If loadAltTree is set to a positive value, the next alternate tree redo branch is loaded. If loadAltTree is set to a negative value, the previous alternate tree redo branch is loaded.

## 10. ReaScript runtime & helpers (prototyping)

- **lua_defer** — Lua: `reaper.defer(function)`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to runloop().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **lua_runloop** — Lua: `reaper.runloop(function)`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to defer().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **lua_atexit** — Lua: `reaper.atexit(function)`
  Adds code to be executed when the script finishes or is ended by the user. Typically used to clean up after the user terminates defer() or runloop() code.
- **lua_set_action_options** — Lua: `reaper.set_action_options(flag)`
  reaper.set_action_options(flag)
  Sets action options for the script.
  flag&1: script will auto-terminate if re-launched while already running
  flag&2: if (flag&1) is set, script will re-launch after auto-terminating. otherwise, re-launch is ignored.
  flag&4: set script toggle state on
  flag&8: set script toggle state off
- **lua_get_action_context** — Lua: `reaper.get_action_context()`
  is_new_value,filename,sectionID,cmdID,mode,resolution,val,contextstr = reaper.get_action_context()
  Returns contextual information about the script, typically MIDI/OSC input values.
  val will be set to a relative or absolute value depending on mode (=0: absolute mode, >0: relative modes).
  resolution=127 for 7-bit resolution, =16383 for 14-bit resolution.
  sectionID, and cmdID will be set to -1 if the script is not part of the action list.
  mode, resolution and val will be set to -1 if the script was not triggered via MIDI/OSC.
  contextstr may be empty or one of:
  midi:XX[:YY] (one or two bytes hex)
  [wheel|hwheel|mtvert|mthorz|mtzoom|mtrot|mediakbd]:flags
  key:flags:keycode
  osc:/msg[:f=FloatValue|:s=StringValue]
  KBD_OnMainActionEx
  (flags may include V=virtkey, S=shift, A=alt/option, C=control/command, W=win/control)
- **eel_defer** — EEL2: `defer("code")`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to runloop().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **eel_runloop** — EEL2: `runloop("code")`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to defer().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **eel_atexit** — EEL2: `atexit("code")`
  Adds code to be executed when the script finishes or is ended by the user. Typically used to clean up after the user terminates defer() or runloop() code.
- **eel_set_action_options** — EEL2: `set_action_options(flags)`
  Sets action options for the script.
  flag&1: script will auto-terminate if re-launched while already running
  flag&2: if (flag&1) is set, script will re-launch after auto-terminating. otherwise, re-launch is ignored.
  flag&4: set script toggle state on
  flag&8: set script toggle state off
- **eel_get_action_context** — EEL2: `get_action_context(#filename,sectionID,cmdID,mode,resolution,val,contextstr)`
  Queries contextual information about the script, typically MIDI/OSC input values.
  Returns true if a new value has been updated.
  val will be set to a relative or absolute value depending on mode (=0: absolute mode, >0: relative modes).
  resolution=127 for 7-bit resolution, =16383 for 14-bit resolution.
  sectionID, and cmdID will be set to -1 if the script is not part of the action list.
  mode, resolution and val will be set to -1 if the script was not triggered via MIDI/OSC.
  contextstr may be empty or one of:
  midi:XX[:YY] (one or two bytes hex)
  [wheel|hwheel|mtvert|mthorz|mtzoom|mtrot|mediakbd]:flags
  key:flags:keycode
  osc:/msg[:f=FloatValue|:s=StringValue]
  KBD_OnMainActionEx
  (flags may include V=virtkey, S=shift, A=alt/option, C=control/command, W=win/control)
- **python_defer** — Python: `RPR_defer(String code)`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to runloop().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **python_runloop** — Python: `RPR_runloop(String code)`
  Adds code to be called back by REAPER. Used to create persistent ReaScripts that continue to run and respond to input, while the user does other tasks. Identical to defer().
  Note that no undo point will be automatically created when the script finishes, unless you create it explicitly.
- **python_atexit** — Python: `RPR_atexit(String)`
  Adds code to be executed when the script finishes or is ended by the user. Typically used to clean up after the user terminates defer() or runloop() code.
- **eel_sleep** — EEL2: `sleep(ms)`
  Yields the CPU for the millisecond count specified, calling Sleep() on Windows or usleep() on other platforms.
- **eel_time** — EEL2: `time([&val])`
  Sets the parameter (or a temporary buffer if omitted) to the number of seconds since January 1, 1970, and returns a reference to that value. The granularity of the value returned is 1 second.
- **eel_time_precise** — EEL2: `time_precise([&val])`
  Sets the parameter (or a temporary buffer if omitted) to a system-local timestamp in seconds, and returns a reference to that value. The granularity of the value returned is system defined (but generally significantly smaller than one second).
- **time_precise** — C: `double time_precise()`
  Gets a precise system timestamp in seconds
- **eel_printf** — EEL2: `printf("format"[, ...])`
  Output formatted string to system-specific destination, see sprintf() for more information
- **file_exists** — C: `bool file_exists(const char* path)`
  returns true if path points to a valid, readable file
- **format_timestr** — C: `void format_timestr(double tpos, char* buf, int buf_sz)`
  Format tpos (which is time in seconds) as hh:mm:ss.sss. See format_timestr_pos, format_timestr_len.
- **format_timestr_len** — C: `void format_timestr_len(double tpos, char* buf, int buf_sz, double offset, int modeoverride)`
  time formatting mode overrides: -1=proj default.
  0=time
  1=measures.beats + time
  2=measures.beats
  3=seconds
  4=samples
  5=h:m:s:f
  offset is start of where the length will be calculated from
- **format_timestr_pos** — C: `void format_timestr_pos(double tpos, char* buf, int buf_sz, int modeoverride)`
  time formatting mode overrides: -1=proj default.
  0=time
  1=measures.beats + time
  2=measures.beats
  3=seconds
  4=samples
  5=h:m:s:f
- **parse_timestr** — C: `double parse_timestr(const char* buf)`
  Parse hh:mm:ss.sss time string, return time in seconds (or 0.0 on error). See parse_timestr_pos, parse_timestr_len.
- **parse_timestr_len** — C: `double parse_timestr_len(const char* buf, double offset, int modeoverride)`
  time formatting mode overrides: -1=proj default.
  0=time
  1=measures.beats + time
  2=measures.beats
  3=seconds
  4=samples
  5=h:m:s:f
- **parse_timestr_pos** — C: `double parse_timestr_pos(const char* buf, int modeoverride)`
  Parse time string, time formatting mode overrides: -1=proj default.
  0=time
  1=measures.beats + time
  2=measures.beats
  3=seconds
  4=samples
  5=h:m:s:f
- **mkvolstr** — C: `void mkvolstr(char* strNeed64, double vol)`
- **mkpanstr** — C: `void mkpanstr(char* strNeed64, double pan)`
- **mkvolpanstr** — C: `void mkvolpanstr(char* strNeed64, double vol, double pan)`
- **parsepanstr** — C: `double parsepanstr(const char* str)`
- **DB2SLIDER** — C: `double DB2SLIDER(double x)`
- **SLIDER2DB** — C: `double SLIDER2DB(double y)`
- **get_ini_file** — C: `const char* get_ini_file()`
  Get reaper.ini full filename.
- **set_config_var_string** — C: `int set_config_var_string(const char* name, const char* value, int persist)`
  sets ini configuration variable value as string (converted to the appropriate type), in memory. Return value is 0 if failed, 1 if global preference, 2 if project setting. If persist is set to the correct type (1=global preference, 2=project default), then value will be written to reaper.ini. Note that some ini keys can only be written with persist=3, these will be updated in the .ini file but may not take effect immediately (equivalent of WritePrivateProfileString). Note that when using persist=3, you may delete the key by writing the special value '=delete='
- **get_config_var_string** — C: `bool get_config_var_string(const char* name, char* bufOut, int bufOut_sz)`
  gets ini configuration variable value as string. For most keys, this will get the in-memory copy of it, but for others it will actually read the .ini file directly.
- **genGuid** — C: `void genGuid(GUID* g)`
- **guidToString** — C: `void guidToString(const GUID* g, char* destNeed64)`
  dest should be at least 64 chars long to be safe
- **stringToGuid** — C: `void stringToGuid(const char* str, GUID* g)`
- **ResolveWildcards** — C: `void ResolveWildcards(ReaProject* project, double timePosition, const char* wildcards, char* resolvedStringNeedBig, int resolvedStringNeedBig_sz)`
  Resolve a wildcard string. Any wildcards that are valid in the Big Clock can be resolved using this function. Pass in timePosition=-1 to use the current project playback position.
- **reduce_open_files** — C: `int reduce_open_files(int flags)`
  garbage-collects extra open files and closes them. if flags has 1 set, this is done incrementally (call this from a regular timer, if desired). if flags has 2 set, files are aggressively closed (they may need to be re-opened very soon). returns number of files closed by this call.

## 11. System, console & debugging

- **APIExists** — C: `bool APIExists(const char* function_name)`
  Returns true if function_name exists in the REAPER API
- **APITest** — C: `void APITest()`
  Displays a message window if the API was successfully called.
- **AddRemoveReaScript** — C: `int AddRemoveReaScript(bool add, int sectionID, const char* scriptfn, bool commit)`
  Add a ReaScript (return the new command ID, or 0 if failed) or remove a ReaScript (return >0 on success). Use commit==true when adding/removing a single script. When bulk adding/removing n scripts, you can optimize the n-1 first calls with commit==false and commit==true for the last call.
- **GetAppVersion** — C: `const char* GetAppVersion()`
  Returns app version which may include an OS/arch signifier, such as: "6.17" (windows 32-bit), "6.17/x64" (windows 64-bit), "6.17/OSX64" (macOS 64-bit Intel), "6.17/OSX" (macOS 32-bit), "6.17/macOS-arm64", "6.17/linux-x86_64", "6.17/linux-i686", "6.17/linux-aarch64", "6.17/linux-armv7l", etc
- **GetOS** — C: `const char* GetOS()`
  Returns "Win32", "Win64", "OSX32", "OSX64", "macOS-arm64", or "Other".
- **GetMainHwnd** — C: `HWND GetMainHwnd()`
- **GSC_mainwnd** — C: `int GSC_mainwnd(int t)`
  this is just like win32 GetSysColor() but can have overrides.
- **ShowConsoleMsg** — C: `void ShowConsoleMsg(const char* msg)`
  Show a message to the user (also useful for debugging). Send "\n" for newline, "" to clear the console. Prefix string with "!SHOW:" and text will be added to console without opening the window. See ClearConsole
- **ClearConsole** — C: `void ClearConsole()`
  Clear the ReaScript console. See ShowConsoleMsg
- **ReaScriptError** — C: `void ReaScriptError(const char* errmsg)`
  Causes REAPER to display the error message after the current ReaScript finishes. If called within a Lua context and errmsg has a ! prefix, script execution will be terminated.
- **ShowMessageBox** — C: `int ShowMessageBox(const char* msg, const char* title, int type)`
  type 0=OK,1=OKCANCEL,2=ABORTRETRYIGNORE,3=YESNOCANCEL,4=YESNO,5=RETRYCANCEL : ret 1=OK,2=CANCEL,3=ABORT,4=RETRY,5=IGNORE,6=YES,7=NO
- **MB** — C: `int MB(const char* msg, const char* title, int type)`
  type 0=OK,1=OKCANCEL,2=ABORTRETRYIGNORE,3=YESNOCANCEL,4=YESNO,5=RETRYCANCEL : ret 1=OK,2=CANCEL,3=ABORT,4=RETRY,5=IGNORE,6=YES,7=NO
- **ShowPopupMenu** — C: `void ShowPopupMenu(const char* name, int x, int y, HWND hwndParentOptional, void* ctxOptional, int ctx2Optional, int ctx3Optional)`
  shows a context menu, valid names include: track_input, track_panel, track_area, track_routing, item, ruler, envelope, envelope_point, envelope_item. ctxOptional can be a track pointer for track_*, item pointer for item* (but is optional). for envelope_point, ctx2Optional has point index, ctx3Optional has item index (0=main envelope, 1=first AI). for envelope_item, ctx2Optional has AI index (1=first AI)
- **LocalizeString** — C: `const char* LocalizeString(const char* src_string, const char* section, int flagsOptional)`
  Returns a localized version of src_string, in section section. flags can have 1 set to only localize if sprintf-style formatting matches the original.
- **EnsureNotCompletelyOffscreen** — C: `void EnsureNotCompletelyOffscreen(RECT* rInOut)`
  call with a saved window rect for your window and it'll correct any positioning info.
- **GetTooltipWindow** — C: `HWND GetTooltipWindow()`
  gets a tooltip window,in case you want to ask it for font information. Can return NULL.
- **Help_Set** — C: `void Help_Set(const char* helpstring, bool is_temporary_help)`
- **ExecProcess** — C: `const char* ExecProcess(const char* cmdline, int timeoutmsec)`
  Executes command line, returns NULL on total failure, otherwise the return value, a newline, and then the output of the command. If timeoutmsec is 0, command will be allowed to run indefinitely (recommended for large amounts of returned output). timeoutmsec is -1 for no wait/terminate, -2 for no wait and minimize
- **OscLocalMessageToHost** — C: `void OscLocalMessageToHost(const char* message, const double* valueInOptional)`
  Send an OSC message directly to REAPER. The value argument may be NULL. The message will be matched against the default OSC patterns.
- **GetUnderrunTime** — C: `void GetUnderrunTime(unsigned int* audio_xrunOut, unsigned int* media_xrunOut, unsigned int* curtimeOut)`
  retrieves the last timestamps of audio xrun (yellow-flash, if available), media xrun (red-flash), and the current time stamp (all milliseconds)
- **GetAudioDeviceInfo** — C: `bool GetAudioDeviceInfo(const char* attribute, char* descOut, int descOut_sz)`
  get information about the currently open audio device. attribute can be MODE, IDENT_IN, IDENT_OUT, BSIZE, SRATE, BPS. returns false if unknown attribute or device not open.

## Complete function index (924)

All functions in the source (alphabetical). Not listed above = not part of
the curated subset; look them up in `docs/reaper-api.htm`.

`APIExists`  `APITest`  `AddMediaItemToTrack`  `AddProjectMarker`
`AddProjectMarker2`  `AddRegionOrMarker`  `AddRemoveReaScript`  `AddTakeToMediaItem`
`AddTempoTimeSigMarker`  `AnyTrackSolo`  `ApplyNudge`  `ArmCommand`
`AudioAccessorStateChanged`  `AudioAccessorUpdate`  `AudioAccessorValidateState`  `Audio_Init`
`Audio_IsPreBuffer`  `Audio_IsRunning`  `Audio_Quit`  `BypassFxAllTracks`
`CSurf_FlushUndo`  `CSurf_GetTouchState`  `CSurf_GoEnd`  `CSurf_GoStart`
`CSurf_NumTracks`  `CSurf_OnArrow`  `CSurf_OnFXChange`  `CSurf_OnFwd`
`CSurf_OnInputMonitorChange`  `CSurf_OnInputMonitorChangeEx`  `CSurf_OnMuteChange`  `CSurf_OnMuteChangeEx`
`CSurf_OnPanChange`  `CSurf_OnPanChangeEx`  `CSurf_OnPause`  `CSurf_OnPlay`
`CSurf_OnPlayRateChange`  `CSurf_OnRecArmChange`  `CSurf_OnRecArmChangeEx`  `CSurf_OnRecord`
`CSurf_OnRecvPanChange`  `CSurf_OnRecvVolumeChange`  `CSurf_OnRew`  `CSurf_OnRewFwd`
`CSurf_OnScroll`  `CSurf_OnSelectedChange`  `CSurf_OnSendPanChange`  `CSurf_OnSendVolumeChange`
`CSurf_OnSoloChange`  `CSurf_OnSoloChangeEx`  `CSurf_OnStop`  `CSurf_OnTempoChange`
`CSurf_OnTrackSelection`  `CSurf_OnVolumeChange`  `CSurf_OnVolumeChangeEx`  `CSurf_OnWidthChange`
`CSurf_OnWidthChangeEx`  `CSurf_OnZoom`  `CSurf_ResetAllCachedVolPanStates`  `CSurf_ScrubAmt`
`CSurf_SetAutoMode`  `CSurf_SetPlayState`  `CSurf_SetRepeatState`  `CSurf_SetSurfaceMute`
`CSurf_SetSurfacePan`  `CSurf_SetSurfaceRecArm`  `CSurf_SetSurfaceSelected`  `CSurf_SetSurfaceSolo`
`CSurf_SetSurfaceVolume`  `CSurf_SetTrackListChange`  `CSurf_TrackFromID`  `CSurf_TrackToID`
`CalcMediaSrcLoudness`  `CalculateNormalization`  `ClearAllRecArmed`  `ClearConsole`
`ClearPeakCache`  `ColorFromNative`  `ColorToNative`  `CountActionShortcuts`
`CountAutomationItems`  `CountEnvelopePoints`  `CountEnvelopePointsEx`  `CountMediaItems`
`CountProjectMarkers`  `CountSelectedMediaItems`  `CountSelectedTracks`  `CountSelectedTracks2`
`CountTCPFXParms`  `CountTakeEnvelopes`  `CountTakes`  `CountTempoTimeSigMarkers`
`CountTrackEnvelopes`  `CountTrackMediaItems`  `CountTracks`  `CreateNewMIDIItemInProj`
`CreateTakeAudioAccessor`  `CreateTrackAudioAccessor`  `CreateTrackSend`  `CrossfadeEditor_OnCommand`
`CrossfadeEditor_Show`  `DB2SLIDER`  `DeleteActionShortcut`  `DeleteEnvelopePointEx`
`DeleteEnvelopePointRange`  `DeleteEnvelopePointRangeEx`  `DeleteExtState`  `DeleteProjectMarker`
`DeleteProjectMarkerByIndex`  `DeleteTakeMarker`  `DeleteTakeStretchMarkers`  `DeleteTempoTimeSigMarker`
`DeleteTrack`  `DeleteTrackMediaItem`  `DestroyAudioAccessor`  `DoActionShortcutDialog`
`DockGetPosition`  `DockIsChildOfDock`  `DockWindowActivate`  `DockWindowAdd`
`DockWindowAddEx`  `DockWindowRefresh`  `DockWindowRefreshForHWND`  `DockWindowRemove`
`Dock_UpdateDockID`  `EditTempoTimeSigMarker`  `EnsureNotCompletelyOffscreen`  `EnumInstalledFX`
`EnumPitchShiftModes`  `EnumPitchShiftSubModes`  `EnumProjExtState`  `EnumProjectMarkers`
`EnumProjectMarkers2`  `EnumProjectMarkers3`  `EnumProjects`  `EnumRegionRenderMatrix`
`EnumThemeColors`  `EnumTrackMIDIProgramNames`  `EnumTrackMIDIProgramNamesEx`  `EnumerateFiles`
`EnumerateSubdirectories`  `Envelope_Evaluate`  `Envelope_FormatValue`  `Envelope_GetParentTake`
`Envelope_GetParentTrack`  `Envelope_SortPoints`  `Envelope_SortPointsEx`  `ExecProcess`
`FindTempoTimeSigMarker`  `GR_SelectColor`  `GSC_mainwnd`  `GetActionShortcutDesc`
`GetActiveTake`  `GetAllProjectPlayStates`  `GetAppVersion`  `GetArmedCommand`
`GetAudioAccessorEndTime`  `GetAudioAccessorHash`  `GetAudioAccessorSamples`  `GetAudioAccessorStartTime`
`GetAudioDeviceInfo`  `GetConfigWantsDock`  `GetCurrentProjectInLoadSave`  `GetCursorContext`
`GetCursorContext2`  `GetCursorPosition`  `GetCursorPositionEx`  `GetDisplayedMediaItemColor`
`GetDisplayedMediaItemColor2`  `GetEnvelopeInfo_Value`  `GetEnvelopeName`  `GetEnvelopePoint`
`GetEnvelopePointByTime`  `GetEnvelopePointByTimeEx`  `GetEnvelopePointEx`  `GetEnvelopeScalingMode`
`GetEnvelopeStateChunk`  `GetEnvelopeUIState`  `GetExePath`  `GetExtState`
`GetFXEnvelope`  `GetFocusedFX`  `GetFocusedFX2`  `GetFreeDiskSpaceForRecordPath`
`GetGlobalAutomationOverride`  `GetHZoomLevel`  `GetInputActivityLevel`  `GetInputChannelName`
`GetInputOutputLatency`  `GetItemEditingTime2`  `GetItemFromPoint`  `GetItemProjectContext`
`GetItemStateChunk`  `GetLastColorThemeFile`  `GetLastMarkerAndCurRegion`  `GetLastTouchedFX`
`GetLastTouchedTrack`  `GetMIDIInputName`  `GetMIDIInputNameNoAlias`  `GetMIDIOutputName`
`GetMIDIOutputNameNoAlias`  `GetMainHwnd`  `GetMasterMuteSoloFlags`  `GetMasterTrack`
`GetMasterTrackVisibility`  `GetMaxMidiInputs`  `GetMaxMidiOutputs`  `GetMediaFileMetadata`
`GetMediaItem`  `GetMediaItemInfo_Value`  `GetMediaItemNumTakes`  `GetMediaItemTake`
`GetMediaItemTakeByGUID`  `GetMediaItemTakeInfo_Value`  `GetMediaItemTake_Item`  `GetMediaItemTake_Peaks`
`GetMediaItemTake_Source`  `GetMediaItemTake_Track`  `GetMediaItemTrack`  `GetMediaItem_Track`
`GetMediaSourceFileName`  `GetMediaSourceLength`  `GetMediaSourceNumChannels`  `GetMediaSourceParent`
`GetMediaSourceSampleRate`  `GetMediaSourceType`  `GetMediaTrackInfo_Value`  `GetMixerScroll`
`GetMouseModifier`  `GetMousePosition`  `GetNumAudioInputs`  `GetNumAudioOutputs`
`GetNumMIDIInputs`  `GetNumMIDIOutputs`  `GetNumRegionsOrMarkers`  `GetNumTakeMarkers`
`GetNumTracks`  `GetOS`  `GetOutputChannelName`  `GetOutputLatency`
`GetParentTrack`  `GetPeakFileName`  `GetPeakFileNameEx`  `GetPeakFileNameEx2`
`GetPlayPosition`  `GetPlayPosition2`  `GetPlayPosition2Ex`  `GetPlayPositionEx`
`GetPlayState`  `GetPlayStateEx`  `GetProjExtState`  `GetProjectLength`
`GetProjectName`  `GetProjectPath`  `GetProjectPathEx`  `GetProjectStateChangeCount`
`GetProjectTimeOffset`  `GetProjectTimeSignature`  `GetProjectTimeSignature2`  `GetRegionOrMarker`
`GetRegionOrMarkerInfo_Value`  `GetResourcePath`  `GetSelectedEnvelope`  `GetSelectedMediaItem`
`GetSelectedTrack`  `GetSelectedTrack2`  `GetSelectedTrackEnvelope`  `GetSetAutomationItemInfo`
`GetSetAutomationItemInfo_String`  `GetSetEnvelopeInfo_String`  `GetSetEnvelopeState`  `GetSetEnvelopeState2`
`GetSetItemState`  `GetSetItemState2`  `GetSetMediaItemInfo_String`  `GetSetMediaItemTakeInfo_String`
`GetSetMediaTrackInfo_String`  `GetSetProjectAuthor`  `GetSetProjectGrid`  `GetSetProjectInfo`
`GetSetProjectInfo_String`  `GetSetProjectNotes`  `GetSetRegionOrMarkerInfo_String`  `GetSetRepeat`
`GetSetRepeatEx`  `GetSetTempoTimeSigMarkerBasis`  `GetSetTempoTimeSigMarkerFlag`  `GetSetTrackGroupMembership`
`GetSetTrackGroupMembershipEx`  `GetSetTrackGroupMembershipHigh`  `GetSetTrackSendInfo_String`  `GetSetTrackState`
`GetSetTrackState2`  `GetSet_ArrangeView2`  `GetSet_LoopTimeRange`  `GetSet_LoopTimeRange2`
`GetSubProjectFromSource`  `GetTCPFXParm`  `GetTake`  `GetTakeEnvelope`
`GetTakeEnvelopeByName`  `GetTakeMarker`  `GetTakeName`  `GetTakeNumStretchMarkers`
`GetTakeStretchMarker`  `GetTakeStretchMarkerSlope`  `GetTempoMatchPlayRate`  `GetTempoTimeSigMarker`
`GetThemeColor`  `GetThingFromPoint`  `GetToggleCommandState`  `GetToggleCommandStateEx`
`GetTooltipWindow`  `GetTouchedOrFocusedFX`  `GetTrack`  `GetTrackAutomationMode`
`GetTrackColor`  `GetTrackDepth`  `GetTrackEnvelope`  `GetTrackEnvelopeByChunkName`
`GetTrackEnvelopeByName`  `GetTrackFromPoint`  `GetTrackGUID`  `GetTrackMIDILyrics`
`GetTrackMIDINoteName`  `GetTrackMIDINoteNameEx`  `GetTrackMIDINoteRange`  `GetTrackMediaItem`
`GetTrackName`  `GetTrackNumMediaItems`  `GetTrackNumSends`  `GetTrackReceiveName`
`GetTrackReceiveUIMute`  `GetTrackReceiveUIVolPan`  `GetTrackSendInfo_Value`  `GetTrackSendName`
`GetTrackSendUIMute`  `GetTrackSendUIVolPan`  `GetTrackState`  `GetTrackStateChunk`
`GetTrackUIMute`  `GetTrackUIPan`  `GetTrackUIVolPan`  `GetUnderrunTime`
`GetUserFileName`  `GetUserFileNameForRead`  `GetUserInputs`  `GoToMarker`
`GoToRegion`  `HasExtState`  `HasTrackMIDIPrograms`  `HasTrackMIDIProgramsEx`
`Help_Set`  `InsertAutomationItem`  `InsertEnvelopePoint`  `InsertEnvelopePointEx`
`InsertMedia`  `InsertMediaSection`  `InsertTrackAtIndex`  `InsertTrackInProject`
`IsDarkMode`  `IsMediaExtension`  `IsMediaItemSelected`  `IsProjectDirty`
`IsTrackSelected`  `IsTrackVisible`  `LICE_ClipLine`  `LocalizeString`
`Loop_OnArrow`  `MB`  `MIDIEditorFlagsForTrack`  `MIDIEditor_EnumTakes`
`MIDIEditor_GetActive`  `MIDIEditor_GetMode`  `MIDIEditor_GetSetting_int`  `MIDIEditor_GetSetting_str`
`MIDIEditor_GetTake`  `MIDIEditor_LastFocused_OnCommand`  `MIDIEditor_OnCommand`  `MIDIEditor_SetSetting_int`
`MIDI_CountEvts`  `MIDI_DeleteCC`  `MIDI_DeleteEvt`  `MIDI_DeleteNote`
`MIDI_DeleteTextSysexEvt`  `MIDI_DisableSort`  `MIDI_EnumSelCC`  `MIDI_EnumSelEvts`
`MIDI_EnumSelNotes`  `MIDI_EnumSelTextSysexEvts`  `MIDI_GetAllEvts`  `MIDI_GetCC`
`MIDI_GetCCShape`  `MIDI_GetEvt`  `MIDI_GetGrid`  `MIDI_GetHash`
`MIDI_GetNote`  `MIDI_GetPPQPosFromProjQN`  `MIDI_GetPPQPosFromProjTime`  `MIDI_GetPPQPos_EndOfMeasure`
`MIDI_GetPPQPos_StartOfMeasure`  `MIDI_GetProjQNFromPPQPos`  `MIDI_GetProjTimeFromPPQPos`  `MIDI_GetRecentInputEvent`
`MIDI_GetScale`  `MIDI_GetTextSysexEvt`  `MIDI_GetTrackHash`  `MIDI_InsertCC`
`MIDI_InsertEvt`  `MIDI_InsertNote`  `MIDI_InsertTextSysexEvt`  `MIDI_RefreshEditors`
`MIDI_SelectAll`  `MIDI_SetAllEvts`  `MIDI_SetCC`  `MIDI_SetCCShape`
`MIDI_SetEvt`  `MIDI_SetItemExtents`  `MIDI_SetNote`  `MIDI_SetTextSysexEvt`
`MIDI_Sort`  `Main_OnCommand`  `Main_OnCommandEx`  `Main_SaveProject`
`Main_SaveProjectEx`  `Main_UpdateLoopInfo`  `Main_openProject`  `MarkProjectDirty`
`MarkTrackItemsDirty`  `Master_GetPlayRate`  `Master_GetPlayRateAtTime`  `Master_GetTempo`
`Master_NormalizePlayRate`  `Master_NormalizeTempo`  `MediaExplorerGetLastPlayedFileInfo`  `MediaItemDescendsFromTrack`
`Menu_GetHash`  `MoveEditCursor`  `MoveMediaItemToTrack`  `MuteAllTracks`
`NamedCommandLookup`  `OnPauseButton`  `OnPauseButtonEx`  `OnPlayButton`
`OnPlayButtonEx`  `OnStopButton`  `OnStopButtonEx`  `OpenColorThemeFile`
`OpenMediaExplorer`  `OscLocalMessageToHost`  `PCM_Sink_Enum`  `PCM_Sink_GetExtension`
`PCM_Sink_ShowConfig`  `PCM_Source_BuildPeaks`  `PCM_Source_CreateFromFile`  `PCM_Source_CreateFromFileEx`
`PCM_Source_CreateFromType`  `PCM_Source_Destroy`  `PCM_Source_GetPeaks`  `PCM_Source_GetSectionInfo`
`PluginWantsAlwaysRunFx`  `PreventUIRefresh`  `PromptForAction`  `ReaScriptError`
`RecursiveCreateDirectory`  `RefreshToolbar`  `RefreshToolbar2`  `RemoveTrackSend`
`RenderFileSection`  `ReorderSelectedTracks`  `Resample_EnumModes`  `ResolveWildcards`
`ReverseNamedCommandLookup`  `SLIDER2DB`  `ScaleFromEnvelopeMode`  `ScaleToEnvelopeMode`
`SectionFromUniqueID`  `SelectAllMediaItems`  `SelectProjectInstance`  `SendMIDIMessageToHardware`
`SetActiveTake`  `SetAutomationMode`  `SetCurrentBPM`  `SetCursorContext`
`SetEditCurPos`  `SetEditCurPos2`  `SetEnvelopePoint`  `SetEnvelopePointEx`
`SetEnvelopeStateChunk`  `SetExtState`  `SetGlobalAutomationOverride`  `SetItemStateChunk`
`SetMIDIEditorGrid`  `SetMasterTrackVisibility`  `SetMediaItemInfo_Value`  `SetMediaItemLength`
`SetMediaItemPosition`  `SetMediaItemSelected`  `SetMediaItemTakeInfo_Value`  `SetMediaItemTake_Source`
`SetMediaTrackInfo_Value`  `SetMixerScroll`  `SetMouseModifier`  `SetOnlyTrackSelected`
`SetProjExtState`  `SetProjectGrid`  `SetProjectMarker`  `SetProjectMarker2`
`SetProjectMarker3`  `SetProjectMarker4`  `SetProjectMarkerByIndex`  `SetProjectMarkerByIndex2`
`SetRegionOrMarkerInfo_Value`  `SetRegionRenderMatrix`  `SetTakeMarker`  `SetTakeStretchMarker`
`SetTakeStretchMarkerSlope`  `SetTempoTimeSigMarker`  `SetThemeColor`  `SetToggleCommandState`
`SetTrackAutomationMode`  `SetTrackColor`  `SetTrackMIDILyrics`  `SetTrackMIDINoteName`
`SetTrackMIDINoteNameEx`  `SetTrackSelected`  `SetTrackSendInfo_Value`  `SetTrackSendUIPan`
`SetTrackSendUIVol`  `SetTrackStateChunk`  `SetTrackUIInputMonitor`  `SetTrackUIMute`
`SetTrackUIPan`  `SetTrackUIPolarity`  `SetTrackUIRecArm`  `SetTrackUISolo`
`SetTrackUIVolume`  `SetTrackUIWidth`  `ShowActionList`  `ShowConsoleMsg`
`ShowMessageBox`  `ShowPopupMenu`  `SnapToGrid`  `SoloAllTracks`
`Splash_GetWnd`  `SplitMediaItem`  `StuffMIDIMessage`  `TakeFX_AddByName`
`TakeFX_CopyToTake`  `TakeFX_CopyToTrack`  `TakeFX_Delete`  `TakeFX_EndParamEdit`
`TakeFX_FormatParamValue`  `TakeFX_FormatParamValueNormalized`  `TakeFX_GetChainVisible`  `TakeFX_GetCount`
`TakeFX_GetEnabled`  `TakeFX_GetEnvelope`  `TakeFX_GetFXGUID`  `TakeFX_GetFXName`
`TakeFX_GetFloatingWindow`  `TakeFX_GetFormattedParamValue`  `TakeFX_GetIOSize`  `TakeFX_GetNamedConfigParm`
`TakeFX_GetNumParams`  `TakeFX_GetOffline`  `TakeFX_GetOpen`  `TakeFX_GetParam`
`TakeFX_GetParamEx`  `TakeFX_GetParamFromIdent`  `TakeFX_GetParamIdent`  `TakeFX_GetParamName`
`TakeFX_GetParamNormalized`  `TakeFX_GetParamSectionName`  `TakeFX_GetParameterStepSizes`  `TakeFX_GetPinMappings`
`TakeFX_GetPreset`  `TakeFX_GetPresetIndex`  `TakeFX_GetUserPresetFilename`  `TakeFX_NavigatePresets`
`TakeFX_SetEnabled`  `TakeFX_SetNamedConfigParm`  `TakeFX_SetOffline`  `TakeFX_SetOpen`
`TakeFX_SetParam`  `TakeFX_SetParamNormalized`  `TakeFX_SetPinMappings`  `TakeFX_SetPreset`
`TakeFX_SetPresetByIndex`  `TakeFX_Show`  `TakeIsMIDI`  `ThemeLayout_GetLayout`
`ThemeLayout_GetParameter`  `ThemeLayout_RefreshAll`  `ThemeLayout_SetLayout`  `ThemeLayout_SetParameter`
`TimeMap2_GetDividedBpmAtTime`  `TimeMap2_GetNextChangeTime`  `TimeMap2_QNToTime`  `TimeMap2_beatsToTime`
`TimeMap2_timeToBeats`  `TimeMap2_timeToQN`  `TimeMap_GetDividedBpmAtTime`  `TimeMap_GetMeasureInfo`
`TimeMap_GetMetronomePattern`  `TimeMap_GetTimeSigAtTime`  `TimeMap_QNToMeasures`  `TimeMap_QNToTime`
`TimeMap_QNToTime_abs`  `TimeMap_curFrameRate`  `TimeMap_timeToQN`  `TimeMap_timeToQN_abs`
`ToggleTrackSendUIMute`  `TrackCtl_SetToolTip`  `TrackFX_AddByName`  `TrackFX_CopyToTake`
`TrackFX_CopyToTrack`  `TrackFX_Delete`  `TrackFX_EndParamEdit`  `TrackFX_FormatParamValue`
`TrackFX_FormatParamValueNormalized`  `TrackFX_GetByName`  `TrackFX_GetChainVisible`  `TrackFX_GetCount`
`TrackFX_GetEQ`  `TrackFX_GetEQBandEnabled`  `TrackFX_GetEQParam`  `TrackFX_GetEnabled`
`TrackFX_GetFXGUID`  `TrackFX_GetFXName`  `TrackFX_GetFloatingWindow`  `TrackFX_GetFormattedParamValue`
`TrackFX_GetIOSize`  `TrackFX_GetInstrument`  `TrackFX_GetNamedConfigParm`  `TrackFX_GetNumParams`
`TrackFX_GetOffline`  `TrackFX_GetOpen`  `TrackFX_GetParam`  `TrackFX_GetParamEx`
`TrackFX_GetParamFromIdent`  `TrackFX_GetParamIdent`  `TrackFX_GetParamName`  `TrackFX_GetParamNormalized`
`TrackFX_GetParamSectionName`  `TrackFX_GetParameterStepSizes`  `TrackFX_GetPinMappings`  `TrackFX_GetPreset`
`TrackFX_GetPresetIndex`  `TrackFX_GetRecChainVisible`  `TrackFX_GetRecCount`  `TrackFX_GetUserPresetFilename`
`TrackFX_NavigatePresets`  `TrackFX_SetEQBandEnabled`  `TrackFX_SetEQParam`  `TrackFX_SetEnabled`
`TrackFX_SetNamedConfigParm`  `TrackFX_SetOffline`  `TrackFX_SetOpen`  `TrackFX_SetParam`
`TrackFX_SetParamNormalized`  `TrackFX_SetPinMappings`  `TrackFX_SetPreset`  `TrackFX_SetPresetByIndex`
`TrackFX_Show`  `TrackList_AdjustWindows`  `TrackList_UpdateAllExternalSurfaces`  `Track_GetPeakHoldDB`
`Track_GetPeakInfo`  `Undo_BeginBlock`  `Undo_BeginBlock2`  `Undo_CanRedo2`
`Undo_CanUndo2`  `Undo_DoRedo2`  `Undo_DoUndo2`  `Undo_EndBlock`
`Undo_EndBlock2`  `Undo_GetCurEntry`  `Undo_GetEntryDesc`  `Undo_GetEntryTime`
`Undo_GetNumEntries`  `Undo_IsEntryAltTree`  `Undo_OnStateChange`  `Undo_OnStateChange2`
`Undo_OnStateChangeEx`  `Undo_OnStateChangeEx2`  `Undo_OnStateChange_Item`  `Undo_SetCurPos`
`UpdateArrange`  `UpdateItemInProject`  `UpdateItemLanes`  `UpdateTimeline`
`ValidatePtr`  `ValidatePtr2`  `ViewPrefs`  `adjustZoom`
`eel_abs`  `eel_acos`  `eel_asin`  `eel_atan`
`eel_atan2`  `eel_atexit`  `eel_ceil`  `eel_convolve_c`
`eel_cos`  `eel_defer`  `eel_eval`  `eel_exp`
`eel_extension_api`  `eel_fclose`  `eel_feof`  `eel_fflush`
`eel_fft`  `eel_fft_ipermute`  `eel_fft_permute`  `eel_fft_real`
`eel_fgetc`  `eel_fgets`  `eel_floor`  `eel_fopen`
`eel_fprintf`  `eel_fread`  `eel_freembuf`  `eel_fseek`
`eel_ftell`  `eel_fwrite`  `eel_get_action_context`  `eel_gfx_arc`
`eel_gfx_blit`  `eel_gfx_blitext`  `eel_gfx_blurto`  `eel_gfx_circle`
`eel_gfx_clienttoscreen`  `eel_gfx_deltablit`  `eel_gfx_dock`  `eel_gfx_drawchar`
`eel_gfx_drawnumber`  `eel_gfx_drawstr`  `eel_gfx_getchar`  `eel_gfx_getdropfile`
`eel_gfx_getfont`  `eel_gfx_getimgdim`  `eel_gfx_getpixel`  `eel_gfx_gradrect`
`eel_gfx_init`  `eel_gfx_line`  `eel_gfx_lineto`  `eel_gfx_loadimg`
`eel_gfx_measurechar`  `eel_gfx_measurestr`  `eel_gfx_muladdrect`  `eel_gfx_printf`
`eel_gfx_quit`  `eel_gfx_rect`  `eel_gfx_rectto`  `eel_gfx_roundrect`
`eel_gfx_screentoclient`  `eel_gfx_set`  `eel_gfx_setcursor`  `eel_gfx_setfont`
`eel_gfx_setimgdim`  `eel_gfx_setpixel`  `eel_gfx_showmenu`  `eel_gfx_transformblit`
`eel_gfx_triangle`  `eel_gfx_update`  `eel_gfx_variables`  `eel_ifft`
`eel_ifft_real`  `eel_invsqrt`  `eel_log`  `eel_log10`
`eel_loop`  `eel_match`  `eel_matchi`  `eel_max`
`eel_mem_get_values`  `eel_mem_insert_shuffle`  `eel_mem_multiply_sum`  `eel_mem_set_values`
`eel_memcpy`  `eel_memset`  `eel_min`  `eel_pow`
`eel_printf`  `eel_rand`  `eel_runloop`  `eel_set_action_options`
`eel_sign`  `eel_sin`  `eel_sleep`  `eel_sprintf`
`eel_sqr`  `eel_sqrt`  `eel_stack_exch`  `eel_stack_peek`
`eel_stack_pop`  `eel_stack_push`  `eel_str_delsub`  `eel_str_getchar`
`eel_str_insert`  `eel_str_setchar`  `eel_str_setlen`  `eel_strcat`
`eel_strcmp`  `eel_strcpy`  `eel_strcpy_from`  `eel_strcpy_substr`
`eel_stricmp`  `eel_strlen`  `eel_strncat`  `eel_strncmp`
`eel_strncpy`  `eel_strnicmp`  `eel_tan`  `eel_tcp_close`
`eel_tcp_connect`  `eel_tcp_listen`  `eel_tcp_listen_end`  `eel_tcp_recv`
`eel_tcp_send`  `eel_tcp_set_block`  `eel_time`  `eel_time_precise`
`eel_while`  `file_exists`  `format_timestr`  `format_timestr_len`
`format_timestr_pos`  `genGuid`  `get_config_var_string`  `get_ini_file`
`guidToString`  `image_resolve_fn`  `joystick_create`  `joystick_destroy`
`joystick_enum`  `joystick_getaxis`  `joystick_getbuttonmask`  `joystick_getinfo`
`joystick_getpov`  `joystick_update`  `kbd_enumerateActions`  `kbd_getTextFromCmd`
`lua_atexit`  `lua_defer`  `lua_get_action_context`  `lua_gfx.arc`
`lua_gfx.blit`  `lua_gfx.blitext`  `lua_gfx.blurto`  `lua_gfx.circle`
`lua_gfx.clienttoscreen`  `lua_gfx.deltablit`  `lua_gfx.dock`  `lua_gfx.drawchar`
`lua_gfx.drawnumber`  `lua_gfx.drawstr`  `lua_gfx.getchar`  `lua_gfx.getdropfile`
`lua_gfx.getfont`  `lua_gfx.getimgdim`  `lua_gfx.getpixel`  `lua_gfx.gradrect`
`lua_gfx.init`  `lua_gfx.line`  `lua_gfx.lineto`  `lua_gfx.loadimg`
`lua_gfx.measurechar`  `lua_gfx.measurestr`  `lua_gfx.muladdrect`  `lua_gfx.printf`
`lua_gfx.quit`  `lua_gfx.rect`  `lua_gfx.rectto`  `lua_gfx.roundrect`
`lua_gfx.screentoclient`  `lua_gfx.set`  `lua_gfx.setcursor`  `lua_gfx.setfont`
`lua_gfx.setimgdim`  `lua_gfx.setpixel`  `lua_gfx.showmenu`  `lua_gfx.transformblit`
`lua_gfx.triangle`  `lua_gfx.update`  `lua_gfx_variables`  `lua_gmem_attach`
`lua_gmem_read`  `lua_gmem_write`  `lua_new_array`  `lua_runloop`
`lua_set_action_options`  `lua_{reaper.array}.clear`  `lua_{reaper.array}.convolve`  `lua_{reaper.array}.copy`
`lua_{reaper.array}.fft`  `lua_{reaper.array}.fft_real`  `lua_{reaper.array}.get_alloc`  `lua_{reaper.array}.ifft`
`lua_{reaper.array}.ifft_real`  `lua_{reaper.array}.multiply`  `lua_{reaper.array}.resize`  `lua_{reaper.array}.table`
`midi_init`  `midi_reinit`  `mkpanstr`  `mkvolpanstr`
`mkvolstr`  `my_getViewport`  `parse_timestr`  `parse_timestr_len`
`parse_timestr_pos`  `parsepanstr`  `python_atexit`  `python_defer`
`python_runloop`  `reduce_open_files`  `relative_fn`  `resolve_fn`
`resolve_fn2`  `set_config_var_string`  `stringToGuid`  `time_precise`

## Sources

- `docs/reaper-api.htm` — full REAPER API reference (REAPER v7.82/win64),
  1.1 MB HTML, 4 signature variants + full descriptions per function.
- REAPER Extensions SDK: <https://www.reaper.fm/sdk/plugin/plugin.php>
- Local: `docs/reaper-plugin.md` (architecture), `docs/reaper-dev-env.md` (build env).
