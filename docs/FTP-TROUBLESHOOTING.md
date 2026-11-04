# FTP Server: the computer cannot connect

This page is for **psp5 authentic's FTP Server** (*Settings → Network → FTP Server*) on a PSP with **ARK-4 or ARK-5**. It works the same on both, and on every PSP model, because the Wi-Fi is the PSP's own.

**Is this your problem?** The PSP has joined your Wi-Fi and the FTP screen shows an address (for example `192.168.1.23`) and *Waiting for a computer*, but the computer cannot connect. It says *No route to host*, *Destination host unreachable*, or just times out.
(If the PSP never gets an address and the screen says *Could not connect*, the problem is the PSP's own Wi-Fi connection, not this page: check the connection in the XMB under *Settings → Network Settings*. The PSP's Wi-Fi works on 2.4 GHz networks only.)

The page has two steps. Do step 1 first: it is quick. Step 2 is a workaround for when the computer cannot find the PSP on the network.

- [Step 1: the quick checks](#step-1-the-quick-checks)
- [Step 2: the static ARP workaround](#step-2-the-static-arp-workaround) ([Linux](#linux), [Windows](#windows-10-and-11), [macOS](#macos))
- [If it still does not work](#if-it-still-does-not-work) · [Keeping it working](#keeping-it-working) · [What the FTP screen is telling you](#what-the-ftp-screen-is-telling-you)

## Step 1: the quick checks

1. **Switch off WLAN Power Save** in the PSP's own settings (in the XMB: *Settings → Power Save Settings → WLAN Power Save*). With it on, the PSP's Wi-Fi sleeps between the router's messages and may not answer the computer. psp5 authentic shows a hint
   under *Waiting for a computer* when it is on.
2. **Put the computer and the PSP on the same network.** Not a guest network, and not two different routers.
3. **Switch off client isolation.** Wi-Fi *client isolation* stops wireless devices from talking to each other, and FTP from a computer to the PSP needs exactly that.
   1. Sign in to your router's or access point's administration page (usually by typing the router's address into a web browser; it is often printed on the router).
   2. Find the wireless settings. Depending on the router they are under *Wi-Fi*, *Wireless*, *Advanced*, *Security*, *Guest network* or *Access point*.
   3. Look for a setting called *AP Isolation*, *Access Point Isolation*, *Client Isolation*, *Wireless Client Isolation*, *Station Isolation*, *Intra-BSS Isolation* or *Block access to local network* (often an option of a guest network).
   4. For the Wi-Fi network that the PSP uses, make sure it is **off** (a checkbox unticked, or *No*), and save.
   5. If your router has separate settings for 2.4 GHz and 5 GHz, check the **2.4 GHz** one: that is the PSP's network.

   Routers name these things differently, and some have no such switch at all: see the router's manual or support page. Do **not** open the router to the internet (no port forwarding) for this: the PSP and the computer only have to see each other on your home network.
4. **Use the address and port 21 that the PSP shows**, and passive mode if your FTP program asks (see *FTP Server* in [INSTALL.md](../INSTALL.md)).

If the computer still cannot connect, go on.

## Step 2: the static ARP workaround

On a home network, a computer finds the MAC address that belongs to the PSP's IP address by asking *"who has this address?"* (that is **ARP**). Sometimes that lookup fails: the question or the answer does not get through (the cause can be the router or access point, or the PSP's Wi-Fi sleeping). Then the computer reports that
the host is unreachable although the PSP is connected and its FTP server is running. The workaround is to tell **the computer** which MAC address belongs to the PSP's IP address: a *static ARP entry* (Linux calls it a *permanent neighbour entry*). It is stored on the computer only; nothing on the PSP or the router changes.

> **This is a workaround for one cause**, not a fix for every FTP problem. If you add the entry and the computer can reach the PSP (port 21 answers), then the address lookup was the obstacle. If it does not help, remove the entry again (see the *remove* command for your system).
> It applies when the PSP and the computer are on the same local IPv4 network. On different networks a static ARP entry is not the right fix.

### A. Find the PSP's IP address and MAC address

You need two values. **They are different for everyone, so this page uses placeholders. Do not copy anybody else's addresses.**

- **`<PSP_IP>`**: the address on psp5 authentic's FTP screen (the big number in the middle). It is also in your router's list of connected devices.
- **`<PSP_MAC>`**: the PSP's Wi-Fi MAC address. In psp5 authentic: *Settings → System → Console Information → MAC address*. In the XMB: *Settings → System Settings → System Information*. Your router's list of Wi-Fi clients shows it too.
  Make sure it is the PSP's MAC address, not the router's or the computer's.

Write the MAC address the way your system wants it: with colons on Linux and macOS (`AA:BB:CC:DD:EE:FF`), with hyphens on Windows (`AA-BB-CC-DD-EE-FF`). Type every character carefully: a wrong MAC address sends the traffic to the wrong device.

### B. Optional: see that this is the problem

Start the FTP server on the PSP, try to connect once, and then ask the computer what it knows about the PSP's address:

| System | Command | What you see when the lookup fails |
|---|---|---|
| Linux | `ip neigh show <PSP_IP>` | `INCOMPLETE` or `FAILED` (or no line at all) |
| Windows (PowerShell) | `Get-NetNeighbor -IPAddress "<PSP_IP>"` | `Unreachable` or `Incomplete` (or *No MSFT_NetNeighbor objects found*) |
| macOS | `arp -an` (find the line with `<PSP_IP>`) | `(incomplete)` |

### Linux

**1. Find the Wi-Fi interface name.** Open a terminal and run:

```bash
ip route get <PSP_IP>
```

The word after `dev` is the interface (for example `wlan0`, `wlp2s0` or `wlo1`). Below it is called `<WIFI_INTERFACE>`.

**2. Add the entry** (put in your own three values, without the angle brackets):

```bash
sudo ip neigh replace <PSP_IP> lladdr <PSP_MAC> nud permanent dev <WIFI_INTERFACE>
```

**3. Check it and try again:**

```bash
ip neigh show <PSP_IP>
```

You should see the PSP's address, its MAC address and `PERMANENT`. Now try your FTP program. To test the FTP port by itself (if `nc` is installed):

```bash
nc -vz -w 3 <PSP_IP> 21
```

**4. Remove it later:**

```bash
sudo ip neigh del <PSP_IP> dev <WIFI_INTERFACE>
```

### Windows 10 and 11

**1. Open PowerShell as administrator:** Start → type *PowerShell* → right-click → *Run as administrator*. Find the Wi-Fi adapter's name:

```powershell
Get-NetAdapter | Format-Table Name, Status, ifIndex
```

The adapter that is connected to Wi-Fi is often called `Wi-Fi`. Use its exact name as `<WIFI_INTERFACE>` below.

**2. Add the entry** (the MAC address with hyphens):

```powershell
New-NetNeighbor -InterfaceAlias "<WIFI_INTERFACE>" -IPAddress "<PSP_IP>" -LinkLayerAddress "<PSP_MAC>" -State Permanent
```

If PowerShell says that an entry exists already, remove the old one (step 4) and add it again.

**3. Check it and try again:**

```powershell
Get-NetNeighbor -InterfaceAlias "<WIFI_INTERFACE>" -IPAddress "<PSP_IP>"
```

The link-layer address must be the PSP's and the state `Permanent`. Then try your FTP program. To test the FTP port by itself:

```powershell
Test-NetConnection -ComputerName <PSP_IP> -Port 21
```

`TcpTestSucceeded : True` means that the PSP's FTP server answers.

**4. Remove it later** (in the same administrator window):

```powershell
Remove-NetNeighbor -InterfaceAlias "<WIFI_INTERFACE>" -IPAddress "<PSP_IP>" -Confirm:$false
```

*(Prefer the Command Prompt? In one opened as administrator, `arp -s <PSP_IP> <PSP_MAC>` adds an entry and `arp -d <PSP_IP>` removes it. The PowerShell commands above name the Wi-Fi adapter, which is safer if the computer has more than one network connection.)*

Microsoft's pages: [New-NetNeighbor](https://learn.microsoft.com/en-us/powershell/module/nettcpip/new-netneighbor) and [Remove-NetNeighbor](https://learn.microsoft.com/en-us/powershell/module/nettcpip/remove-netneighbor).

### macOS

**1. Find the Wi-Fi interface name.** Open *Terminal* and run:

```bash
networksetup -listallhardwareports
```

In the section called *Wi-Fi*, the value after *Device* (usually `en0`) is `<WIFI_INTERFACE>`.

**2. Add the entry** (the MAC address with colons; your Mac's password is asked, and it does not show while you type):

```bash
sudo arp -S <PSP_IP> <PSP_MAC> ifscope <WIFI_INTERFACE>
```

**3. Check it and try again:**

```bash
arp -an
```

Find the line with the PSP's address: its hardware address must be the PSP's. Then try your FTP program, or test the port by itself with `nc -vz -w 3 <PSP_IP> 21`.

**4. Remove it later:**

```bash
sudo arp -d <PSP_IP> ifscope <WIFI_INTERFACE>
```

If your macOS version does not accept `ifscope`, leave it out. See the [arp manual page](https://manp.gs/mac/8/arp).

### Phones and tablets

A static ARP entry normally cannot be added on a phone or tablet (it needs special access to the device). Use a computer for the transfer, or copy the files with *Settings → Storage → USB Mode* (a cable, no network needed) or a Memory Stick reader.

## If it still does not work

- Check every character of the PSP's IP address and MAC address again.
- Check that the FTP screen is still open and shows the same address (the PSP may have got another one).
- Check step 1 again: client isolation, guest networks and *WLAN Power Save*, for the 2.4 GHz network that the PSP uses.
- If the entry is there but port 21 still does not answer, this workaround is not the answer for your network: look at the router, or use *USB Mode* instead.

## Keeping it working

The entry lives on the computer where you added it. **If the PSP gets another IP address, the entry points to the wrong one and has to be made again.** To avoid that, you can give the PSP a fixed address with a **DHCP reservation** in your router (it hands out the same address to the same
MAC address every time; pick an address that no other device has).

How long the entry lasts depends on the system: on Linux and macOS it is normally gone after a restart of the computer or when you change network; on Windows it stays until you remove it (Microsoft's page says the entry is saved for restarts too). If FTP stops working again, look at the entry (step 3 above) and add it again.
If you only wanted to try it, remove it afterwards.

## What the FTP screen is telling you

- **A program that has connected but does nothing for five minutes is disconnected** (the server answers `421 Timeout` to that one session). That is not the server stopping: it keeps listening, and you can connect again. A file that is being sent or received is never cut off by this.
- **The server stops when the PSP loses its Wi-Fi connection.** Then the screen says *The connection was lost* (✕ starts it again, Back leaves). Look at the PSP's connection (was it disconnected by the router, did someone switch the Wi-Fi switch off?). It has nothing to do with the five minutes above.
- If the screen says *The FTP server is not running*, the Wi-Fi window was closed without choosing a connection: press ✕ and choose one.

There is no password: anyone on the same network can reach your Memory Stick while the FTP screen is open (and `ef0` on a PSP Go). Use it at home, and press Back when you are done.
