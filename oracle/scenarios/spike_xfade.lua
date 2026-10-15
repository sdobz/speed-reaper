-- spike_xfade.lua — M9 spike: crossfade behavior under the oracle (record-only,
-- no behavior asserts). Layout: track 0 = A [0,5] + B [4,9] (overlap [4,5]);
-- track 1 = C [0,5] + D [5,10] (abutting at 5). Probes:
--   (a) 40916 "Crossfade items within time selection" over overlap / over boundary
--   (b) fade lengths set by API on both items
--   config vars: autoxfade-style names + "default crossfade length" read/write
--   action ids: 41059, 43776, 43777, 41191/41192/41193, 40509/40510,
--               41118/41119, 41116 (cycle editing modes)
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/spike_xfade_tone.wav"
local function write_tone(path)
  local f = io.open(path, "wb")
  if not f then return false end
  local sr, secs = 44100, 8
  local ns = sr * secs
  local hdr = "RIFF" .. le32(36 + ns * 2) .. "WAVEfmt " .. le32(16)
              .. le16(1) .. le16(sr) .. le32(sr * 2) .. le16(2) .. le16(16)
              .. "data" .. le32(ns * 2)
  f:write(hdr)
  for i = 0, ns - 1 do
    local s = math.sin(2 * math.pi * 440 * i / sr) * 16000
    f:write(le16(math.floor(s) + 32768))
  end
  f:close()
  return true
end

local FIELDS = { "D_POSITION", "D_LENGTH", "D_FADEINLEN", "D_FADEOUTLEN",
  "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO", "C_FADEINSHAPE", "C_FADEOUTSHAPE",
  "B_FADEIN_AUTO", "B_FADEOUT_AUTO", "D_CROSSFADELEN", "D_CROSSFADENEXT",
  "B_UISEL" }

local items = {}  -- [1]=A [2]=B [3]=C [4]=D
local names = { "A", "B", "C", "D" }

local function itemline(tag, idx)
  local it = items[idx]
  local line = ""
  for _, f in ipairs(FIELDS) do
    local v = reaper.GetMediaItemInfo_Value(it, f)
    line = line .. " " .. f .. "=" .. tostring(v)
  end
  record(tag, line)
end

local function record_all(tag)
  for i = 1, 4 do itemline(tag .. "_" .. names[i], i) end
end

-- NOTE: this REAPER export only has the *_string config var APIs
-- (no get_config_var_double/int), so all config probes go through
-- get_config_var_string (numbers come back as their stored string form).
local function cfg(tag, name)
  local ok, v
  ok, v = reaper.get_config_var_string(name)
  record(tag, (ok and tostring(v) or "ABSENT"))
end

local function record_cfgs(prefix)
  cfg(prefix .. ".defaut_xfade", "defaut_xfade")
  cfg(prefix .. ".default_xfade", "default_xfade")
  cfg(prefix .. ".xfade_len", "xfade_len")
  cfg(prefix .. ".default_crossfade", "default_crossfade")
  cfg(prefix .. ".autoxfade", "autoxfade")
  cfg(prefix .. ".autocrossfade", "autocrossfade")
  cfg(prefix .. ".xfade_shape", "xfade_shape")
  cfg(prefix .. ".crossfade", "crossfade")
  record(prefix .. ".toggle.41118", tostring(reaper.GetToggleCommandState(41118)))
  record(prefix .. ".toggle.41119", tostring(reaper.GetToggleCommandState(41119)))
end

local function select(idxs)
  for i = 1, 4 do
    reaper.SetMediaItemInfo_Value(items[i], "B_UISEL",
      (idxs and idxs[i] and 1) or 0)
  end
  reaper.UpdateArrange()
end

local function clear_fades()
  for i = 1, 4 do
    local it = items[i]
    reaper.SetMediaItemInfo_Value(it, "D_FADEINLEN", 0)
    reaper.SetMediaItemInfo_Value(it, "D_FADEOUTLEN", 0)
    reaper.SetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO", 0)
    reaper.SetMediaItemInfo_Value(it, "D_FADEOUTLEN_AUTO", 0)
  end
  reaper.UpdateArrange()
end


local function later(fn)
  reaper.defer(function()
    local ok, err = pcall(fn)
    if not ok then
      record("chain_error", tostring(err))
      check("probe done", true, "")
      finish()
    end
  end)
end

local function step(label, cmd, fn)
  reaper.Main_OnCommand(cmd, 0)
  reaper.UpdateArrange()
  later(function()
    local ok, err = pcall(fn)
    if not ok then
      record("chain_error_at_" .. label, tostring(err))
      check("probe done", true, "")
      finish()
    end
  end)
end

start("settled", function()
  record("wav_written", tostring(write_tone(WAV)))
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  local n = reaper.CountTracks(nil)
  for _ = 1, 2 - n do reaper.InsertTrackAtIndex(n, true) end
  local pos = { [1] = 0.0, [2] = 4.0, [3] = 0.0, [4] = 5.0 }
  for i = 1, 4 do
    local tr = reaper.GetTrack(nil, math.floor((i - 1) / 2))
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos[i])
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 5.0)
    items[i] = it
  end
  reaper.UpdateArrange()
  later(function()
    record("reaper_version", reaper.GetAppVersion())
    record_all("baseline")
    record_cfgs("cfg_baseline")

    -- (a1) 40916 with tsel over the A/B overlap [4,5]
    reaper.GetSet_LoopTimeRange2(nil, true, false, 4.0, 5.0, false)
    select({ [1] = true, [2] = true })
    later(function()
      step("40916_overlap", 40916, function()
        record_all("after40916_overlap")

        -- (a2) 40916 with tsel over the C/D abutting boundary [4.5,5.5]
        clear_fades()
        reaper.GetSet_LoopTimeRange2(nil, true, false, 4.5, 5.5, false)
        select({ [3] = true, [4] = true })
        later(function()
          step("40916_boundary", 40916, function()
            record_all("after40916_boundary")

            -- (b) fade lengths set by API on both items of each pair
            clear_fades()
            local function setfade(i, fin, fout)
              local it = items[i]
              if fin then
                reaper.SetMediaItemInfo_Value(it, "D_FADEINLEN", fin)
                reaper.SetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO", 0)
              end
              if fout then
                reaper.SetMediaItemInfo_Value(it, "D_FADEOUTLEN", fout)
                reaper.SetMediaItemInfo_Value(it, "D_FADEOUTLEN_AUTO", 0)
              end
            end
            setfade(1, nil, 0.5) -- A fadeout
            setfade(2, 0.5, nil) -- B fadein
            setfade(3, nil, 0.3) -- C fadeout
            setfade(4, 0.3, nil) -- D fadein
            reaper.UpdateArrange()
            later(function()
              record_all("after_api_set")

              -- 43776 Set crossfade length to default (overlap pair)
              select({ [1] = true, [2] = true })
              later(function()
                step("43776_AB", 43776, function()
                  record_all("after43776_AB")
                  select({ [3] = true, [4] = true })
                  later(function()
                    step("43776_CD", 43776, function()
                      record_all("after43776_CD")

                      -- 43777 Set crossfade shape to default (all four)
                      select({ [1] = true, [2] = true, [3] = true, [4] = true })
                      later(function()
                        step("43777_all", 43777, function()
                          record_all("after43777_all")

                          -- does 43776 act on AUTO-flagged fades (manual ones were
                          -- untouched above)?
                          select({ [1] = true, [2] = true })
                          reaper.SetMediaItemInfo_Value(items[1], "D_FADEOUTLEN", 0.0)
                          reaper.SetMediaItemInfo_Value(items[1], "D_FADEOUTLEN_AUTO", 1)
                          reaper.SetMediaItemInfo_Value(items[2], "D_FADEINLEN", 0.0)
                          reaper.SetMediaItemInfo_Value(items[2], "D_FADEINLEN_AUTO", 1)
                          reaper.UpdateArrange()
                          later(function()
                            step("43776_auto_AB", 43776, function()
                              record_all("after43776_auto_AB")

                          -- 40509 Fade items in to cursor (cursor 6.0)
                          clear_fades()
                          select({ [2] = true, [4] = true })
                          reaper.SetEditCurPos(6.0, true, false)
                          reaper.UpdateArrange()
                          later(function()
                            step("40509_in", 40509, function()
                              record_all("after40509_in")

                              -- 40510 Fade items out from cursor (cursor 6.0)
                              clear_fades()
                              select({ [1] = true, [2] = true, [4] = true })
                              reaper.SetEditCurPos(6.0, true, false)
                              reaper.UpdateArrange()
                              later(function()
                                step("40510_out", 40510, function()
                                  record_all("after40510_out")

                                  -- 41191 Remove fade in
                                  clear_fades()
                                  select({ [2] = true, [4] = true })
                                  reaper.SetMediaItemInfo_Value(items[2], "D_FADEINLEN", 0.7)
                                  reaper.SetMediaItemInfo_Value(items[4], "D_FADEINLEN", 0.2)
                                  reaper.UpdateArrange()
                                  later(function()
                                    step("41191_rmfin", 41191, function()
                                      record_all("after41191_rmfin")

                                      -- 41192 Remove fade out
                                      select({ [1] = true, [3] = true })
                                      reaper.SetMediaItemInfo_Value(items[1], "D_FADEOUTLEN", 0.7)
                                      reaper.SetMediaItemInfo_Value(items[3], "D_FADEOUTLEN", 0.2)
                                      reaper.UpdateArrange()
                                      later(function()
                                        step("41192_rmfout", 41192, function()
                                          record_all("after41192_rmfout")

                                          -- 41193 Remove fade in and out
                                          select({ [1] = true, [2] = true, [3] = true, [4] = true })
                                          reaper.SetMediaItemInfo_Value(items[1], "D_FADEINLEN", 0.4)
                                          reaper.SetMediaItemInfo_Value(items[2], "D_FADEINLEN", 0.4)
                                          reaper.SetMediaItemInfo_Value(items[3], "D_FADEOUTLEN", 0.4)
                                          reaper.SetMediaItemInfo_Value(items[4], "D_FADEOUTLEN", 0.4)
                                          reaper.UpdateArrange()
                                          later(function()
                                            step("41193_rmboth", 41193, function()
                                              record_all("after41193_rmboth")

                                              -- 41059 Crossfade any overlapping items
                                              clear_fades()
                                              select({ [1] = true, [2] = true })
                                              later(function()
                                                step("41059_AB", 41059, function()
                                                  record_all("after41059_AB")
                                                  clear_fades()
                                                  select({ [3] = true, [4] = true })
                                                  later(function()
                                                    step("41059_CD", 41059, function()
                                                      record_all("after41059_CD")

                                                      -- 41119 disable / 41118 enable auto-crossfades
                                                      clear_fades()
                                                      select({ [3] = true, [4] = true })
                                                      later(function()
                                                        step("41119_disable", 41119, function()
                                                          record_cfgs("cfg_after_disable")
                                                          step("41118_enable", 41118, function()
                                                            record_cfgs("cfg_after_enable")

                                                            -- 41116 cycle editing modes (3x -> original)
                                                            record_cfgs("cfg_mode_m0")
                                                            step("41116_m1", 41116, function()
                                                              record_cfgs("cfg_mode_m1")
                                                              step("41116_m2", 41116, function()
                                                                record_cfgs("cfg_mode_m2")
                                                                step("41116_m3", 41116, function()
                                                                  record_cfgs("cfg_mode_m3")

                                                                  -- write test for the
                                                                  -- default-crossfade-length var (string API only)
                                                                  local ok0, v0 =
                                                                    reaper.get_config_var_string("defaut_xfade")
                                                                  if ok0 then
                                                                    local wok, werr = pcall(reaper.set_config_var_string,
                                                                      "defaut_xfade", "0.75", 0)
                                                                    record("cfg_write.set_rc", (wok and "ok") or ("ERR:" .. tostring(werr)))
                                                                    reaper.UpdateArrange()
                                                                    later(function()
                                                                      local ok1, v1 = reaper.get_config_var_string("defaut_xfade")
                                                                      record("cfg_write.defaut_xfade_orig", tostring(v0))
                                                                      record("cfg_write.defaut_xfade_after_set", (ok1 and tostring(v1)) or "ABSENT")
                                                                      pcall(reaper.set_config_var_string,
                                                                        "defaut_xfade", tostring(v0), 0)
                                                                      reaper.UpdateArrange()
                                                                      later(function()
                                                                        local ok2, v2 = reaper.get_config_var_string("defaut_xfade")
                                                                        record("cfg_write.defaut_xfade_restored", (ok2 and tostring(v2)) or "ABSENT")
                                                                        check("probe done", true, "")
                                                                        finish()
                                                                      end)
                                                                    end)
                                                                  else
                                                                    -- var absent in this fresh home: test whether it can
                                                                    -- be CREATED by a write, then reset to REAPER's
                                                                    -- factory default 0.2 to keep the home clean
                                                                    record("cfg_write.defaut_xfade_orig", "ABSENT")
                                                                    local wok, werr = pcall(reaper.set_config_var_string,
                                                                      "defaut_xfade", "0.75", 0)
                                                                    record("cfg_write.set_rc", (wok and "ok") or ("ERR:" .. tostring(werr)))
                                                                    reaper.UpdateArrange()
                                                                    later(function()
                                                                      local ok1, v1 = reaper.get_config_var_string("defaut_xfade")
                                                                      record("cfg_write.defaut_xfade_after_set", (ok1 and tostring(v1)) or "ABSENT")
                                                                      pcall(reaper.set_config_var_string, "defaut_xfade", "0.2", 0)
                                                                      reaper.UpdateArrange()
                                                                      later(function()
                                                                        local ok2, v2 = reaper.get_config_var_string("defaut_xfade")
                                                                        record("cfg_write.defaut_xfade_reset", (ok2 and tostring(v2)) or "ABSENT")
                                                                        check("probe done", true, "")
                                                                        finish()
                                                                      end)
                                                                    end)
                                                                  end
                                                                end)
                                                              end)
                                                            end)
                                                          end)
                                                        end)
                                                      end)
                                                    end)
                                                  end)
                                                end)
                                              end)
                                            end)
                                          end)
                                        end)
                                      end)
                                    end)
                                  end)
                                end)
                              end)
                            end)
                          end)
                        end)
                      end)
                    end)
                  end)
                end)
              end)
            end)
          end)
        end)
      end)
    end)
  end)
  end)
  end)
end)
