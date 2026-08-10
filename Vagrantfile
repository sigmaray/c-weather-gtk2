# -*- mode: ruby -*-
# vi: set ft=ruby :
#
# Windows XP SP3 guest for building/running c-weather-gtk2.
# Requires local box from `just box` (packaging/xp-box/bake.sh).

Vagrant.configure("2") do |config|
  config.vm.box = "c-weather-gtk2/windows-xp-sp3"
  config.vm.guest = :windows
  config.vm.communicator = "winrm"

  config.winrm.username = "vagrant"
  config.winrm.password = "vagrant"
  config.winrm.transport = :plaintext
  config.winrm.basic_auth_only = true
  config.winrm.timeout = 1800
  config.vm.boot_timeout = 1800

  config.vm.synced_folder ".", "/vagrant"

  # XP has no mklink; map project share to V: on every boot.
  config.vm.provision "shell", run: "always", inline: <<-SHELL
    net use V: \\\\vboxsvr\\vagrant /persistent:yes 2>nul
  SHELL

  config.vm.provider "virtualbox" do |vb|
    vb.name = "c-weather-gtk2-xp"
    vb.gui = false
    vb.memory = 1024
    vb.cpus = 1
    vb.linked_clone = true
    vb.customize ["modifyvm", :id, "--ioapic", "off"]
    vb.customize ["modifyvm", :id, "--vram", "32"]
    vb.customize ["modifyvm", :id, "--clipboard", "bidirectional"]
  end
end
