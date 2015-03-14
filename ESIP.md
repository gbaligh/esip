# REGISTER HANDLER #

ESIP can handle REGISTER request, but without authentication process. It send a 200 Ok directly to the UAC.
## TODO ##
  * Check REGISTER validity
  * Authentication according to a default Login/Password
  * Update database with new user
  * Show registred users using CLI command


# INVITE HANDLER #

ESIP is not really handling INVITE, it send 200 OK for all INVITEs received.
## TODO ##
  * Check INVITE
  * ADD SDP handler
  * Create a DIALOG FSM after 200 Ok is sent
  * Send INVITE using CLI command with predefined SDP body