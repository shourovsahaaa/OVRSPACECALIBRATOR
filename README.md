<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo_light.png?raw=true">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo_dark.png?raw=true">
  <img alt="Space Calibrator" src="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo.png?raw=true">
</picture>

This program is designed to allow you to synchronise multiple playspaces with one another in SteamVR. This fork of Space Calibrator (spacecal) also supports [continuous calibration](#continuous-calibration).

Continuous calibration is a tracking mode which automatically aligns playspaces together, using a tracker on the headset.

## Installing

### Steam

> [!NOTE]  
> **Space Calibrator is also available to Steam.**

You may find [Space Calibrator on Steam here](https://s.team/a/3368750).

### From GitHub

To install Space Calibrator, please get the latest installer from the downloads page, and install it. Make sure that you have:
- Installed [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- Installed SteamVR and run it at least once with a VR headset connected.
- SteamVR is not running before you run the installer. If SteamVR is running the installer will not be able to install Space Calibrator correctly.

## Calibration

If you do not wish to use continuous calibration, you will have to use regular calibration. This means that every so often you will have to sync your headset's playspace with your tracker's playspace.

To calibrate:
1. Copy the chaperone/guardian bounds from your HMD's play space
   > You will only have to do this once. Connect your VR headset and start SteamVR. Then go to space calibrator's window (it will be minimised), and click the "Copy Chaperone" button.

2. Open the SteamVR dashboard. At the bottom, click on the Space Calibrator icon.
3. In the Space Calibrator overlay, you'll see two lists at the top. On the left `Reference Space` column, select the controller you'll be calibrating along (e.g. Quest controller, Pico controller). On the right `Target Space`, select your SteamVR tracker (e.g. Vive Ultimate Tracker, Vive Tracker 3.0, Vive Ultimate Tracker). You can use the Identify button to make the controllers blink and tracker LEDs flash to see if you've selected the correct ones.
4. Click the "Start calibration" button, and start calibrating.

## Continuous Calibration

> [!IMPORTANT]  
> **A tracker attached on your headset is required for this.**

To enable continuous calibration mode, first select your headset on the left column, then the tracker on your headset on the right column. Once you've done so, click `Start Calibration`, and click cancel. Then click `Continuous Calibration` to enable continuous calibration.

1. Start SteamVR with the VR headset you wish to use.
2. Turn on **ONLY** the tracker which is attached on the VR headset.
3. Select the VR headset and tracker and calibrate.
4. Turn on your other devices.
5. You should see them line up with you as you after moving around your playspace for a bit for an initial calibration.


## Help

If you need help with setting up this program, please check the [wiki](https://github.com/pushrax/OpenVR-SpaceCalibrator/wiki), or join the [Discord server](https://discord.gg/ja3WgNjC3z).
