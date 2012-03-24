#!/bin/sh

# Use this script to autogenerate cpp wrappers for dbus calls
#
# We expect that you have built qtmoko in directory build in
# the same folder as qtmoko git.
QDBUSXML2CPP=qofonodbusxml2cpp/qofonodbusxml2cpp

$QDBUSXML2CPP -p qofonomanager ofono.xml org.ofono.Manager
$QDBUSXML2CPP -p qofonomodem ofono.xml org.ofono.Modem
$QDBUSXML2CPP -p qofonosimmanager ofono.xml org.ofono.SimManager
$QDBUSXML2CPP -p qofonovoicecallmanager ofono.xml org.ofono.VoiceCallManager
$QDBUSXML2CPP -p qofonoradiosettings ofono.xml org.ofono.RadioSettings
$QDBUSXML2CPP -p qofonophonebook ofono.xml org.ofono.Phonebook
$QDBUSXML2CPP -p qofonomessagemanager ofono.xml org.ofono.MessageManager
$QDBUSXML2CPP -p qofonopushnotification ofono.xml org.ofono.PushNotification
$QDBUSXML2CPP -p qofonosupplementaryservices ofono.xml org.ofono.SupplementaryServices
$QDBUSXML2CPP -p qofononetworkregistration ofono.xml org.ofono.NetworkRegistration
$QDBUSXML2CPP -p qofonocellbroadcast ofono.xml org.ofono.CellBroadcast
$QDBUSXML2CPP -p qofonoconnectionmanager ofono.xml org.ofono.ConnectionManager
$QDBUSXML2CPP -p qofononetworkoperator ofono-operator.xml org.ofono.NetworkOperator
$QDBUSXML2CPP -p qofonovoicecall ofono-voicecall.xml org.ofono.VoiceCall
