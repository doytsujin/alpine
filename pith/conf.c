#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: conf.c 551 2007-05-01 17:44:08Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

/*======================================================================
     conf.c
     Implements the Pine configuration management routines
  ====*/


#include "../pith/headers.h"
#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/state.h"
#include "../pith/remote.h"
#include "../pith/keyword.h"
#include "../pith/mailview.h"
#include "../pith/list.h"
#include "../pith/status.h"
#include "../pith/ldap.h"
#include "../pith/folder.h"
#include "../pith/thread.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/pattern.h"
#include "../pith/color.h"
#include "../pith/options.h"
#include "../pith/busy.h"
#include "../pith/readfile.h"
#include "../pith/hist.h"
#include "../pith/charconv/utf8.h"
#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif


#define	TO_BAIL_THRESHOLD	60


/*
 * Internal prototypes
 */
void     convert_configvars_to_utf8(struct variable *, char *);
void     convert_configvar_to_utf8(struct variable *, char *);
void     set_current_pattern_vals(struct pine *);
void     convert_pattern_data(void);
void     convert_filts_pattern_data(void);
void     convert_scores_pattern_data(void);
void     convert_pinerc_patterns(long);
void     convert_pinerc_filts_patterns(long);
void     convert_pinerc_scores_patterns(long);
void	 set_old_growth_bits(struct pine *, int);
int	 test_old_growth_bits(struct pine *, int);
int      var_is_in_rest_of_file(char *, char *);
char    *skip_over_this_var(char *, char *);
char    *native_nl(char *);
void	 free_pinerc_lines(PINERC_LINE **);
void     set_color_val(struct variable *, int);
int      copy_localfile_to_remotefldr(RemType, char *, char *, void *, char **);
void	 panic1(char *, char *);
char    *backcompat_convert_from_utf8(char *, size_t, char *);
#ifdef	_WINDOWS
char    *transformed_color(char *);
int      convert_pc_gray_names(struct pine *, PINERC_S *, EditWhich);
int      unix_color_style_in_pinerc(PINERC_S *);
char	*pcpine_general_help(char *);
char    *pcpine_help(HelpType);  /* defined in alpine/help */
#endif	/* _WINDOWS */


/* hook too allow caller to decide what to do about failure */
int	(*pith_opt_remote_pinerc_failure)(void);


/*------------------------------------
Some definitions to keep the static "variable" array below 
a bit more readable...
  ----*/
CONF_TXT_T cf_text_comment[] = "#\n# Alpine configuration file\n#\n# This file sets the configuration options used by Alpine and PC-Alpine. These\n# options are usually set from within Alpine or PC-Alpine. There may be a\n# system-wide configuration file which sets the defaults for some of the\n# variables. On Unix, run alpine -conf to see how system defaults have been set.\n# For variables that accept multiple values, list elements are\
 separated by\n# commas. A line beginning with a space or tab is considered to be a\n# continuation of the previous line. For a variable to be unset its value must\n# be blank. To set a variable to the empty string its value should be \"\".\n# You can override system defaults by setting a variable to the empty string.\n# Lines beginning with \"#\" are comments, and ignored by Alpine.\n";


CONF_TXT_T cf_text_personal_name[] =	"Over-rides your full name from Unix password file. Required for PC-Alpine.";

CONF_TXT_T cf_text_user_id[] =		"Your login/e-mail user name";

CONF_TXT_T cf_text_user_domain[] =		"Sets domain part of From: and local addresses in outgoing mail.";

CONF_TXT_T cf_text_smtp_server[] =		"List of SMTP servers for sending mail. If blank: Unix Alpine uses sendmail.";

CONF_TXT_T cf_text_nntp_server[] =		"NNTP server for posting news. Also sets news-collections for news reading.";

#ifdef	ENABLE_LDAP
CONF_TXT_T cf_text_ldap_server[] =		"LDAP servers for looking up addresses.";
#endif	/* ENABLE_LDAP */

CONF_TXT_T cf_text_wp_indexheight[] =		"WebAlpine index table row height";

CONF_TXT_T cf_text_wp_indexlines[] =		"WebAlpine number of index lines in table";

CONF_TXT_T cf_text_wp_aggstate[] =		"WebAlpine aggregate operations tab state";

CONF_TXT_T cf_text_wp_state[] =			"WebAlpine various aspects of cross-session state";

CONF_TXT_T cf_text_wp_columns[] =		"WebAlpine preferred width for message display in characters";

CONF_TXT_T cf_text_inbox_path[] =		"Path of (local or remote) INBOX, e.g. ={mail.somewhere.edu}inbox\n# Normal Unix default is the local INBOX (usually /usr/spool/mail/$USER).";

CONF_TXT_T cf_text_incoming_folders[] =	"List of incoming msg folders besides INBOX, e.g. ={host2}inbox, {host3}inbox\n# Syntax: optnl-label {optnl-imap-host-name}folder-path";

CONF_TXT_T cf_text_folder_collections[] =	"List of directories where saved-message folders may be. First one is\n# the default for Saves. Example: Main {host1}mail/[], Desktop mail\\[]\n# Syntax: optnl-label {optnl-imap-hostname}optnl-directory-path[]";

CONF_TXT_T cf_text_news_collections[] =	"List, only needed if nntp-server not set, or news is on a different host\n# than used for NNTP posting. Examples: News *[] or News *{host3/nntp}[]\n# Syntax: optnl-label *{news-host/protocol}[]";

CONF_TXT_T cf_text_pruned_folders[] =	"List of folders, assumed to be in first folder collection,\n# offered for pruning each month.  For example: mumble";

CONF_TXT_T cf_text_default_fcc[] =		"Over-rides default path for sent-mail folder, e.g. =old-mail (using first\n# folder collection dir) or ={host2}sent-mail or =\"\" (to suppress saving).\n# Default: sent-mail (Unix) or SENTMAIL.MTX (PC) in default folder collection.";

CONF_TXT_T cf_text_default_saved[] =	"Over-rides default path for saved-msg folder, e.g. =saved-messages (using 1st\n# folder collection dir) or ={host2}saved-mail or =\"\" (to suppress saving).\n# Default: saved-messages (Unix) or SAVEMAIL.MTX (PC) in default collection.";

CONF_TXT_T cf_text_postponed_folder[] =	"Over-rides default path for postponed messages folder, e.g. =pm (which uses\n# first folder collection dir) or ={host4}pm (using home dir on host4).\n# Default: postponed-msgs (Unix) or POSTPOND.MTX (PC) in default fldr coltn.";

CONF_TXT_T cf_text_mail_directory[] =	"Alpine compares this value with the first folder collection directory.\n# If they match (or no folder collections are defined), and the directory\n# does not exist, Alpine will create and use it. Default: ~/mail";

CONF_TXT_T cf_text_read_message_folder[] =	"If set, specifies where already-read messages will be moved upon quitting.";

CONF_TXT_T cf_text_form_letter_folder[] =	"If set, specifies where form letters should be stored.";

CONF_TXT_T cf_text_signature_file[] =	"Over-rides default path for signature file. Default is ~/.signature";

CONF_TXT_T cf_text_literal_sig[] =	"Contains the actual signature contents as opposed to the signature filename.\n# If defined, this overrides the signature-file. Default is undefined.";

CONF_TXT_T cf_text_global_address_book[] =	"List of file or path names for global/shared addressbook(s).\n# Default: none\n# Syntax: optnl-label path-name";

CONF_TXT_T cf_text_address_book[] =	"List of file or path names for personal addressbook(s).\n# Default: ~/.addressbook (Unix) or \\PINE\\ADDRBOOK (PC)\n# Syntax: optnl-label path-name";

CONF_TXT_T cf_text_feature_list[] =	"List of features; see Alpine's Setup/options menu for the current set.\n# e.g. feature-list= select-without-confirm, signature-at-bottom\n# Default condition for all of the features is no-.";

CONF_TXT_T cf_text_initial_keystroke_list[] =	"Alpine executes these keys upon startup (e.g. to view msg 13: i,j,1,3,CR,v)";

CONF_TXT_T cf_text_default_composer_hdrs[] =	"Only show these headers (by default) when composing messages";

CONF_TXT_T cf_text_customized_hdrs[] =	"Add these customized headers (and possible default values) when composing";

CONF_TXT_T cf_text_view_headers[] =	"When viewing messages, include this list of headers";

CONF_TXT_T cf_text_view_margin_left[] =	"When viewing messages, number of blank spaces between left display edge and text";

CONF_TXT_T cf_text_view_margin_right[] =	"When viewing messages, number of blank spaces between right display edge and text";

CONF_TXT_T cf_text_quote_suppression[] =	"When viewing messages, number of lines of quote displayed before suppressing";

CONF_TXT_T cf_text_wordsep[] =	"When these characters appear in the middle of a word in the composer\n# the forward word function will stop at the first text following (as happens\n# with SPACE characters by default)";

CONF_TXT_T cf_text_color_style[] =	"Controls display of color";

CONF_TXT_T cf_text_current_indexline_style[] =	"Controls display of color for current index line";

CONF_TXT_T cf_text_titlebar_color_style[] =	"Controls display of color for the titlebar at top of screen";

CONF_TXT_T cf_text_view_hdr_color[] =	"When viewing messages, these are the header colors";

CONF_TXT_T cf_text_save_msg_name_rule[] =	"Determines default folder name for Saves...\n# Choices: default-folder, by-sender, by-from, by-recipient, last-folder-used.\n# Default: \"default-folder\", i.e. \"saved-messages\" (Unix) or \"SAVEMAIL\" (PC).";

CONF_TXT_T cf_text_fcc_name_rule[] =	"Determines default name for Fcc...\n# Choices: default-fcc, by-recipient, last-fcc-used.\n# Default: \"default-fcc\" (see also \"default-fcc=\" variable.)";

CONF_TXT_T cf_text_sort_key[] =		"Sets presentation order of messages in Index. Choices:\n# Subject, From, Arrival, Date, Size, To, Cc, OrderedSubj, Score, and Thread.\n# Order may be reversed by appending /Reverse. Default: \"Arrival\".";

CONF_TXT_T cf_text_addrbook_sort_rule[] =	"Sets presentation order of address book entries. Choices: dont-sort,\n# fullname-with-lists-last, fullname, nickname-with-lists-last, nickname\n# Default: \"fullname-with-lists-last\".";

CONF_TXT_T cf_text_folder_sort_rule[] =	"Sets presentation order of folder list entries. Choices: alphabetical,\n# alpha-with-dirs-last, alpha-with-dirs-first.\n# Default: \"alpha-with-directories-last\".";

CONF_TXT_T cf_text_old_char_set[] =	"Character-set is obsolete, use display-character-set, keyboard-character-set,\n# and posting-character-set.";

CONF_TXT_T cf_text_disp_char_set[] =	"Reflects capabilities of the display you have.\n# If unset, the default is taken from your locale. That is usually the right\n# thing to use. Typical alternatives include UTF-8, ISO-8859-x, and EUC-JP\n# (where x is a number between 1 and 9).";

CONF_TXT_T cf_text_key_char_set[] =	"Reflects capabilities of the keyboard you have.\n# If unset, the default is to use the same value\n# used for the display-character-set.";

CONF_TXT_T cf_text_post_character_set[] =	"Defaults to UTF-8. This is used for outgoing messages.\n# It is usually correct to leave this unset.";

CONF_TXT_T cf_text_editor[] =		"Specifies the program invoked by ^_ in the Composer,\n# or the \"enable-alternate-editor-implicitly\" feature.";

CONF_TXT_T cf_text_speller[] =		"Specifies the program invoked by ^T in the Composer.";

CONF_TXT_T cf_text_deadlets[] =		"Specifies the number of dead letter files to keep when canceling.";

CONF_TXT_T cf_text_fillcol[] =		"Specifies the column of the screen where the composer should wrap.";

CONF_TXT_T cf_text_replystr[] =		"Specifies the string to insert when replying to a message.";

CONF_TXT_T cf_text_quotereplstr[] =    	"Specifies the string to replace quotes with when viewing a message.";

CONF_TXT_T cf_text_replyintro[] =	"Specifies the introduction to insert when replying to a message.";

CONF_TXT_T cf_text_emptyhdr[] =		"Specifies the string to use when sending a  message with no to or cc.";

CONF_TXT_T cf_text_image_viewer[] =	"Program to view images (e.g. GIF or TIFF attachments).";

CONF_TXT_T cf_text_browser[] =		"List of programs to open Internet URLs (e.g. http or ftp references).";

CONF_TXT_T cf_text_inc_startup[] =	"Sets message which cursor begins on. Choices: first-unseen, first-recent,\n# first-important, first-important-or-unseen, first-important-or-recent,\n# first, last. Default: \"first-unseen\".";

CONF_TXT_T cf_pruning_rule[] =		"Allows a default answer for the prune folder questions. Choices: yes-ask,\n# yes-no, no-ask, no-no, ask-ask, ask-no. Default: \"ask-ask\".";

CONF_TXT_T cf_reopen_rule[] =		"Controls behavior when reopening an already open folder.";

CONF_TXT_T cf_text_thread_disp_style[] = "Style that MESSAGE INDEX is displayed in when threading.";

CONF_TXT_T cf_text_thread_index_style[] = "Style of THREAD INDEX or default MESSAGE INDEX when threading.";

CONF_TXT_T cf_text_thread_more_char[] =	"When threading, character used to indicate collapsed messages underneath.";

CONF_TXT_T cf_text_thread_exp_char[] =	"When threading, character used to indicate expanded messages underneath.";

CONF_TXT_T cf_text_thread_lastreply_char[] =	"When threading, character used to indicate this is the last reply\n# to the parent of this message.";

CONF_TXT_T cf_text_use_only_domain_name[] = "If \"user-domain\" not set, strips hostname in FROM address. (Unix only)";

CONF_TXT_T cf_text_printer[] =		"Your default printer selection";

CONF_TXT_T cf_text_personal_print_command[] =	"List of special print commands";

CONF_TXT_T cf_text_personal_print_cat[] =	"Which category default print command is in";

CONF_TXT_T cf_text_standard_printer[] =	"The system wide standard printers";

CONF_TXT_T cf_text_last_time_prune_quest[] =	"Set by Alpine; controls beginning-of-month sent-mail pruning.";

CONF_TXT_T cf_text_last_version_used[] =	"Set by Alpine; controls display of \"new version\" message.";

CONF_TXT_T cf_text_disable_drivers[] =		"List of mail drivers to disable.";

CONF_TXT_T cf_text_disable_auths[] =		"List of SASL authenticators to disable.";

CONF_TXT_T cf_text_remote_abook_metafile[] =	"Set by Alpine; contains data for caching remote address books.";

CONF_TXT_T cf_text_old_patterns[] =		"Patterns is obsolete, use patterns-xxx";

CONF_TXT_T cf_text_old_filters[] =		"Patterns-filters is obsolete, use patterns-filters2";

CONF_TXT_T cf_text_old_scores[] =		"Patterns-scores is obsolete, use patterns-scores2";

CONF_TXT_T cf_text_patterns[] =			"Patterns and their actions are stored here.";

CONF_TXT_T cf_text_remote_abook_history[] =	"How many extra copies of remote address book should be kept. Default: 3";

CONF_TXT_T cf_text_remote_abook_validity[] =	"Minimum number of minutes between checks for remote address book changes.\n# 0 means never check except when opening a remote address book.\n# -1 means never check. Default: 5";

CONF_TXT_T cf_text_bugs_fullname[] =	"Full name for bug report address used by \"Report Bug\" command";

CONF_TXT_T cf_text_bugs_address[] =	"Email address used to send bug reports";

CONF_TXT_T cf_text_bugs_extras[] =		"Program/Script used by \"Report Bug\" command. No default.";

CONF_TXT_T cf_text_suggest_fullname[] =	"Full name for suggestion address used by \"Report Bug\" command";

CONF_TXT_T cf_text_suggest_address[] =	"Email address used to send suggestions";

CONF_TXT_T cf_text_local_fullname[] =	"Full name for \"local support\" address used by \"Report Bug\" command.\n# Default: Local Support";

CONF_TXT_T cf_text_local_address[] =	"Email address used to send to \"local support\".\n# Default: postmaster";

CONF_TXT_T cf_text_forced_abook[] =	"Force these address book entries into all writable personal address books.\n# Syntax is   forced-abook-entry=nickname|fullname|address\n# This is a comma-separated list of entries, each with syntax above.\n# Existing entries with same nickname are not replaced.\n# Example: help|Help Desk|help@ourdomain.com";

CONF_TXT_T cf_text_kblock_passwd[] =	"This is a number between 1 and 5.  It is the number of times a user will\n# have to enter a password when they run the keyboard lock command in the\n# main menu.  Default is 1.";

CONF_TXT_T cf_text_sendmail_path[] =	"This names the path to an alternative program, and any necessary arguments,\n# to be used in posting mail messages.  Example:\n#                    /usr/lib/sendmail -oem -t -oi\n# or,\n#                    /usr/local/bin/sendit.sh\n# The latter a script found in Alpine distribution's contrib/util directory.\n# NOTE: The program MUST read the message to be posted on standard input,\n#       AND operate in the style of sendmail's \"-t\" option.";

CONF_TXT_T cf_text_oper_dir[] =	"This names the root of the tree to which the user is restricted when reading\n# and writing folders and files.  For example, on Unix ~/work confines the\n# user to the subtree beginning with their work subdirectory.\n# (Note: this alone is not sufficient for preventing access.  You will also\n# need to restrict shell access and so on, see Alpine Technical Notes.)\n# Default: not set (so no restriction)";

CONF_TXT_T cf_text_in_fltr[] = 		"This variable takes a list of programs that message text is piped into\n# after MIME decoding, prior to display.";

CONF_TXT_T cf_text_out_fltr[] =		"This defines a program that message text is piped into before MIME\n# encoding, prior to sending";

CONF_TXT_T cf_text_alt_addrs[] =	"A list of alternate addresses the user is known by";

CONF_TXT_T cf_text_keywords[] =		"A list of keywords for use in categorizing messages";

CONF_TXT_T cf_text_kw_colors[] =	"Colors used to display keywords in the index";

CONF_TXT_T cf_text_kw_braces[] =	"Characters which surround keywords in SUBJKEY token.\n# Default is \"{\" \"} \"";

CONF_TXT_T cf_text_abook_formats[] =	"This is a list of formats for address books.  Each entry in the list is made\n# up of space-delimited tokens telling which fields are displayed and in\n# which order.  See help text";

CONF_TXT_T cf_text_index_format[] =	"This gives a format for displaying the index.  It is made\n# up of space-delimited tokens telling which fields are displayed and in\n# which order.  See help text";

CONF_TXT_T cf_text_overlap[] =		"The number of lines of overlap when scrolling through message text";

CONF_TXT_T cf_text_maxremstreams[] =	"The maximum number of non-stayopen remote connections that Alpine will use";

CONF_TXT_T cf_text_permlocked[] =	"A list of folders that should be left open once opened (INBOX is implicit)";

CONF_TXT_T cf_text_margin[] =		"Number of lines from top and bottom of screen where single\n# line scrolling occurs.";

CONF_TXT_T cf_text_stat_msg_delay[] =	"The number of seconds to sleep after writing a status message";

CONF_TXT_T cf_text_busy_cue_rate[] =	"Number of times per-second to update busy cue messages";

CONF_TXT_T cf_text_mailcheck[] =	"The approximate number of seconds between checks for new mail";

CONF_TXT_T cf_text_mailchecknoncurr[] =	"The approximate number of seconds between checks for new mail in folders\n# other than the current folder and inbox.\n# Default is same as mail-check-interval";

CONF_TXT_T cf_text_maildropcheck[] =	"The minimum number of seconds between checks for new mail in a Mail Drop.\n# This is always effectively at least as large as the mail-check-interval";

CONF_TXT_T cf_text_nntprange[] =	"For newsgroups accessed using NNTP, only messages numbered in the range\n# lastmsg-range+1 to lastmsg will be considered";

CONF_TXT_T cf_text_news_active[] =	"Path and filename of news configuration's active file.\n# The default is typically \"/usr/lib/news/active\".";

CONF_TXT_T cf_text_news_spooldir[] =	"Directory containing system's news data.\n# The default is typically \"/usr/spool/news\"";

CONF_TXT_T cf_text_upload_cmd[] =	"Path and filename of the program used to upload text from your terminal\n# emulator's into Alpine's composer.";

CONF_TXT_T cf_text_upload_prefix[] =	"Text sent to terminal emulator prior to invoking the program defined by\n# the upload-command variable.\n# Note: _FILE_ will be replaced with the temporary file used in the upload.";

CONF_TXT_T cf_text_download_cmd[] =	"Path and filename of the program used to download text via your terminal\n# emulator from Alpine's export and save commands.";

CONF_TXT_T cf_text_download_prefix[] =	"Text sent to terminal emulator prior to invoking the program defined by\n# the download-command variable.\n# Note: _FILE_ will be replaced with the temporary file used in the downlaod.";

CONF_TXT_T cf_text_goto_default[] =	"Sets the default folder and collectionoffered at the Goto Command's prompt.";

CONF_TXT_T cf_text_mailcap_path[] =	"Sets the search path for the mailcap configuration file.\n# NOTE: colon delimited under UNIX, semi-colon delimited under DOS/Windows/OS2.";

CONF_TXT_T cf_text_mimetype_path[] =	"Sets the search path for the mimetypes configuration file.\n# NOTE: colon delimited under UNIX, semi-colon delimited under DOS/Windows/OS2.";

CONF_TXT_T cf_text_newmail_fifo_path[] = "Sets the filename for the newmail fifo (named pipe). Unix only.";

CONF_TXT_T cf_text_nmw_width[] = "Sets the width for the NewMail screen.";

CONF_TXT_T cf_text_user_input_timeo[] =	"If no user input for this many hours, Alpine will exit if in an idle loop\n# waiting for a new command.  If set to zero (the default), then there will\n# be no timeout.";

CONF_TXT_T cf_text_debug_mem[] =	"Debug-memory is obsolete";

CONF_TXT_T cf_text_tcp_open_timeo[] =	"Sets the time in seconds that Alpine will attempt to open a network\n# connection.  The default is 30, the minimum is 5, and the maximum is\n# system defined (typically 75).";

CONF_TXT_T cf_text_tcp_read_timeo[] =	"Network read warning timeout. The default is 15, the minimum is 5, and the\n# maximum is 1000.";

CONF_TXT_T cf_text_tcp_write_timeo[] =	"Network write warning timeout. The default is 0 (unset), the minimum\n# is 5 (if not 0), and the maximum is 1000.";

CONF_TXT_T cf_text_tcp_query_timeo[] =	"If this much time has elapsed at the time of a tcp read or write\n# timeout, Alpine will ask if you want to break the connection.\n# Default is 60 seconds, minimum is 5, maximum is 1000.";

CONF_TXT_T cf_text_rsh_open_timeo[] =	"Sets the time in seconds that Alpine will attempt to open a UNIX remote\n# shell connection.  The default is 15, min is 5, and max is unlimited.\n# Zero disables rsh altogether.";

CONF_TXT_T cf_text_rsh_path[] =		"Sets the name of the command used to open a UNIX remote shell connection.\n# The default is typically /usr/ucb/rsh.";

CONF_TXT_T cf_text_rsh_command[] =	"Sets the format of the command used to open a UNIX remote\n# shell connection.  The default is \"%s %s -l %s exec /etc/r%sd\"\n# NOTE: the 4 (four) \"%s\" entries MUST exist in the provided command\n# where the first is for the command's path, the second is for the\n# host to connect to, the third is for the user to connect as, and the\n# fourth is for the connection method (typically \"imap\")";

CONF_TXT_T cf_text_ssh_open_timeo[] =	"Sets the time in seconds that Alpine will attempt to open a UNIX secure\n# shell connection.  The default is 15, min is 5, and max is unlimited.\n# Zero disables ssh altogether.";

CONF_TXT_T cf_text_inc_check_timeo[] =	"Sets the time in seconds that Alpine will attempt to open a network\n# connection when checking for new unseen messages in an incoming folder.\n#  The default is 5.";

CONF_TXT_T cf_text_inc_check_interval[] = "Sets the approximate number of seconds between checks for unseen messages\n# in incoming folders. The default is 180.";

CONF_TXT_T cf_text_inc_check_list[] =	"List of incoming folders to check for unseen messages. The default if left\n# blank is to check all incoming folders.";

CONF_TXT_T cf_text_ssh_path[] =		"Sets the name of the command used to open a UNIX secure shell connection.\n# Typically this is /usr/bin/ssh.";

CONF_TXT_T cf_text_ssh_command[] =	"Sets the format of the command used to open a UNIX secure\n# shell connection.  The default is \"%s %s -l %s exec /etc/r%sd\"\n# NOTE: the 4 (four) \"%s\" entries MUST exist in the provided command\n# where the first is for the command's path, the second is for the\n# host to connect to, the third is for the user to connect as, and the\n# fourth is for the connection method (typically \"imap\")";

CONF_TXT_T cf_text_version_threshold[] = "Sets the version number Alpine will use as a threshold for offering\n# its new version message on startup.";

CONF_TXT_T cf_text_archived_folders[] =	"List of folder pairs; the first indicates a folder to archive, and the\n# second indicates the folder read messages in the first should\n# be moved to.";

CONF_TXT_T cf_text_elm_style_save[] =	"Elm-style-save is obsolete, use saved-msg-name-rule";

CONF_TXT_T cf_text_header_in_reply[] =	"Header-in-reply is obsolete, use include-header-in-reply in feature-list";

CONF_TXT_T cf_text_feature_level[] =	"Feature-level is obsolete, use feature-list";

CONF_TXT_T cf_text_old_style_reply[] =	"Old-style-reply is obsolete, use signature-at-bottom in feature-list";

CONF_TXT_T cf_text_compose_mime[] =	"Compose-mime is obsolete";

CONF_TXT_T cf_text_show_all_characters[] =	"Show-all-characters is obsolete";

CONF_TXT_T cf_text_save_by_sender[] =	"Save-by-sender is obsolete, use saved-msg-name-rule";

CONF_TXT_T cf_text_file_dir[] =		"Default directory used for Attachment handling (attach and save)\n# and Export command output";

CONF_TXT_T cf_text_folder_extension[] =	"Folder-extension is obsolete";

CONF_TXT_T cf_text_normal_foreground_color[] =	"Choose: black, blue, green, cyan, red, magenta, yellow, or white.";

CONF_TXT_T cf_text_window_position[] =	"Window position in the format: CxR+X+Y\n# Where C and R are the window size in characters and X and Y are the\n# screen position of the top left corner of the window.\n# This is no longer used unless position is not set in registry.";

CONF_TXT_T cf_text_newsrc_path[] =		"Full path and name of NEWSRC file";


/*----------------------------------------------------------------------
These are the variables that control a number of pine functions.  They
come out of the .pinerc and the /usr/local/lib/pine.conf files.  Some can
be set by the user while in Alpine.  Eventually all the local ones should
be so and maybe the global ones too.

Each variable can have a command-line, user, global, and current value.
All of these values are malloc'd.  The user value is the one read out of
the user's .pinerc, the global value is the one from the system pine
configuration file.  There are often defaults for the global values, set
at the start of init_vars().  Perhaps someday there will be group values.
The current value is the one that is actually in use.
  ----*/
/* name                                                  is_changed_val
                                                       remove_quotes  |
                                                     is_outermost  |  |
                                                   is_onlymain  |  |  |
                                                   is_fixed  |  |  |  |
                                                 is_list  |  |  |  |  |
                                            is_global  |  |  |  |  |  |
                                           is_user  |  |  |  |  |  |  |
                                   been_written  |  |  |  |  |  |  |  |
                                     is_used  |  |  |  |  |  |  |  |  |
                              is_obsolete  |  |  |  |  |  |  |  |  |  |
                                        |  |  |  |  |  |  |  |  |  |  |
  (on following line) description       |  |  |  |  |  |  |  |  |  |  |
                                |       |  |  |  |  |  |  |  |  |  |  |
                                |       |  |  |  |  |  |  |  |  |  |  |  */
static struct variable variables[] = {
    {"Personal-Name",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_personal_name},
#if defined(DOS) || defined(OS2)
                        /* Have to have this on DOS, PC's, Macs, etc... */
{"User-ID",				0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
#else			/* Don't allow on UNIX machines for some security */
{"User-ID",				0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0,
#endif
				cf_text_user_id},
{"User-Domain",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_user_domain},
{"SMTP-Server",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_smtp_server},
{"NNTP-Server",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_nntp_server},
{"Inbox-Path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_inbox_path},
{"Incoming-Archive-Folders",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_archived_folders},
{"Pruned-Folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_pruned_folders},
{"Default-Fcc",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_default_fcc},
{"Default-Saved-Msg-Folder",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_default_saved},
{"Postponed-Folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_postponed_folder},
{"Read-Message-Folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_read_message_folder},
{"Form-Letter-Folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_form_letter_folder},
{"Literal-Signature",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_literal_sig},
{"Signature-File",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_signature_file},
{"Feature-List",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_feature_list},
{"Initial-Keystroke-List",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_initial_keystroke_list},
{"Default-Composer-Hdrs",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_default_composer_hdrs},
{"Customized-Hdrs",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_customized_hdrs},
{"Viewer-Hdrs",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_view_headers},
{"Viewer-Margin-Left",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_view_margin_left},
{"Viewer-Margin-Right",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_view_margin_right},
{"Quote-Suppression-Threshold",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_quote_suppression},
{"Saved-Msg-Name-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_save_msg_name_rule},
{"Fcc-Name-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_fcc_name_rule},
{"Sort-Key",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_sort_key},
{"Addrbook-Sort-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_addrbook_sort_rule},
{"Folder-Sort-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_folder_sort_rule},
{"Goto-Default-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_goto_default},
{"Incoming-Startup-Rule",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_inc_startup},
{"Pruning-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_pruning_rule},
{"Folder-Reopen-Rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_reopen_rule},
{"Threading-Display-Style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_thread_disp_style},
{"Threading-Index-Style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_thread_index_style},
{"Threading-Indicator-Character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_thread_more_char},
{"Threading-Expanded-Character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_thread_exp_char},
{"Threading-Lastreply-Character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_thread_lastreply_char},
{"Display-Character-Set",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_disp_char_set},
{"Character-Set",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_old_char_set},
{"Keyboard-Character-Set",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_key_char_set},
{"Posting-Character-Set",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_post_character_set},
{"Editor",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_editor},
{"Speller",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_speller},
{"Composer-Wrap-Column",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_fillcol},
{"Reply-Indent-String",			0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
				cf_text_replystr},
{"Reply-Leadin",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_replyintro},
{"Quote-Replace-String",		0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
				cf_text_quotereplstr},
{"Composer-Word-Separators",		0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_wordsep},
{"Empty-Header-Message",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_emptyhdr},
{"Image-Viewer",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_image_viewer},
{"Use-Only-Domain-Name",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_use_only_domain_name},
{"Bugs-Fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_bugs_fullname},
{"Bugs-Address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_bugs_address},
{"Bugs-Additional-Data",		0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_bugs_extras},
{"Suggest-Fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_suggest_fullname},
{"Suggest-Address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_suggest_address},
{"Local-Fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_local_fullname},
{"Local-Address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_local_address},
{"Forced-Abook-Entry",			0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0,
				cf_text_forced_abook},
{"Kblock-Passwd-Count",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_kblock_passwd},
{"Display-Filters",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_in_fltr},
{"Sending-Filters",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_out_fltr},
{"Alt-Addresses",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_alt_addrs},
{"Keywords",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_keywords},
{"Keyword-Surrounding-Chars",		0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
				cf_text_kw_braces},
{"Addressbook-Formats",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_abook_formats},
{"Index-Format",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_index_format},
{"Viewer-Overlap",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_overlap},
{"Scroll-Margin",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_margin},
{"Status-Message-Delay",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_stat_msg_delay},
{"Busy-Cue-Rate",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_busy_cue_rate},
{"Mail-Check-Interval",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_mailcheck},
{"Mail-Check-Interval-Noncurrent",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_mailchecknoncurr},
{"Maildrop-Check-Minimum",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_maildropcheck},
{"NNTP-Range",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_nntprange},
{"Newsrc-Path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_newsrc_path},
{"News-Active-File-Path",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_news_active},
{"News-Spool-Directory",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_news_spooldir},
{"Upload-Command",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_upload_cmd},
{"Upload-Command-Prefix",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_upload_prefix},
{"Download-Command",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_download_cmd},
{"Download-Command-Prefix",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_download_prefix},
{"Mailcap-Search-Path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_mailcap_path},
{"Mimetype-Search-Path",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_mimetype_path},
{"URL-Viewers",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_browser},
{"Max-Remote-Connections",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_maxremstreams},
{"Stay-Open-Folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_permlocked},
{"Incoming-Check-Timeout",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_inc_check_timeo},
{"Incoming-Check-Interval",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_inc_check_interval},
{"Incoming-Check-List",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_inc_check_list},
{"Dead-Letter-Files",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_deadlets},
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
{"Newmail-FIFO-Path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_newmail_fifo_path},
#endif
{"Newmail-Window-Width",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_nmw_width},
/*
 * Starting here, the variables are hidden in the Setup/Config screen.
 * They are exposed if feature expose-hidden-config is set.
 */
{"Incoming-Folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_incoming_folders},
{"Mail-Directory",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_mail_directory},
{"Folder-Collections",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_folder_collections},
{"News-Collections",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_news_collections},
{"Address-Book",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_address_book},
{"Global-Address-Book",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_global_address_book},
{"Standard-Printer",			0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0,
				cf_text_standard_printer},
{"Last-Time-Prune-Questioned",		0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0,
				cf_text_last_time_prune_quest},
{"Last-Version-Used",			0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0,
				cf_text_last_version_used},
{"Sendmail-Path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_sendmail_path},
{"Operating-Dir",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_oper_dir},
{"User-Input-Timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_user_input_timeo},
/* OBSOLETE */
#ifdef DEBUGJOURNAL
{"Debug-Memory",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_debug_mem},
#endif

{"TCP-Open-Timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_tcp_open_timeo},
{"TCP-Read-Warning-Timeout",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_tcp_read_timeo},
{"TCP-Write-Warning-Timeout",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_tcp_write_timeo},
{"TCP-Query-Timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_tcp_query_timeo},
{"Rsh-Command",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_rsh_command},
{"Rsh-Path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_rsh_path},
{"Rsh-Open-Timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_rsh_open_timeo},
{"Ssh-Command",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_ssh_command},
{"Ssh-Path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_ssh_path},
{"Ssh-Open-Timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_ssh_open_timeo},
{"New-Version-Threshold",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_version_threshold},
{"Disable-These-Drivers",		0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_disable_drivers},
{"Disable-These-Authenticators",	0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_disable_auths},
{"Remote-Abook-Metafile",		0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0,
				cf_text_remote_abook_metafile},
{"Remote-Abook-History",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_remote_abook_history},
{"Remote-Abook-Validity",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_remote_abook_validity},
{"Printer",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_printer},
{"Personal-Print-Command",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_personal_print_command},
{"Personal-Print-Category",		0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
				cf_text_personal_print_cat},
{"Patterns",				1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_old_patterns},
{"Patterns-Roles",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_patterns},
{"Patterns-Filters2",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_patterns},
{"Patterns-Filters",			1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_old_filters},
{"Patterns-Scores2",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_patterns},
{"Patterns-Scores",			1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_old_scores},
{"Patterns-Indexcolors",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_patterns},
{"Patterns-Other",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_patterns},

/* OBSOLETE VARS */
{"Elm-Style-Save",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_elm_style_save},
{"Header-in-Reply",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_header_in_reply},
{"Feature-Level",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_feature_level},
{"Old-Style-Reply",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_old_style_reply},
{"Compose-Mime",			1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0,
				cf_text_compose_mime},
{"Show-All-Characters",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_show_all_characters},
{"Save-By-Sender",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_save_by_sender},
#if defined(DOS) || defined(OS2)
{"File-Directory",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_file_dir},
{"Folder-Extension",			1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_folder_extension},
#endif
#ifndef	_WINDOWS
{"Color-Style",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_color_style},
#endif
{"Current-Indexline-Style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_current_indexline_style},
{"Titlebar-Color-Style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_titlebar_color_style},
{"Normal-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_normal_foreground_color},
{"Normal-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Reverse-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Reverse-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Title-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Title-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Title-closed-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Title-closed-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Status-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Status-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Keylabel-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Keylabel-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Keyname-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Keyname-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Selectable-Item-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Selectable-Item-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Meta-Message-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Meta-Message-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote1-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote1-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote2-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote2-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote3-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Quote3-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Signature-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Signature-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Prompt-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Prompt-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-to-me-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-to-me-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-important-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-important-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-deleted-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-deleted-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-answered-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-answered-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-new-Foreground-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-new-Background-Color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-recent-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-recent-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-unseen-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-unseen-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-arrow-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-arrow-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-opening-Foreground-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Index-opening-Background-Color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Viewer-Hdr-Colors",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_view_hdr_color},
{"Keyword-Colors",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0,
				cf_text_kw_colors},
#if defined(DOS) || defined(OS2)
#ifdef _WINDOWS
{"Font-Name",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				"Name and size of font."},
{"Font-Size",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Font-Style",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Font-Char-Set",      			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Print-Font-Name",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				"Name and size of printer font."},
{"Print-Font-Size",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Print-Font-Style",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Print-Font-Char-Set",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
{"Window-Position",			0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0,
				cf_text_window_position},
{"Cursor-Style",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, NULL},
#endif	/* _WINDOWS */
#endif	/* DOS */
#ifdef	ENABLE_LDAP
{"LDAP-Servers",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
				cf_text_ldap_server},
#endif	/* ENABLE_LDAP */
{"wp-indexheight", 			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_wp_indexheight},
{"wp-indexlines",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_wp_indexlines},
{"wp-aggstate",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_wp_aggstate},
{"wp-state",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
				cf_text_wp_state},
{"wp-columns",				0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
				cf_text_wp_columns},
{NULL,					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL}
};


void
init_init_vars(struct pine *ps)
{
    ps->vars = variables;
}


#define DSIZE (25000)
/* this is just like dprint except it prints to a char * */
#ifdef DEBUG
#define   mprint(n,x) {			\
	       if(debug >= (n)){	\
		   snprintf x ;		\
		   db += strlen(db);	\
	       }			\
	   }
#else
#define   mprint(n,x)
#endif

/*
 * this was split out from init_vars so we can get at the
 * pinerc location sooner.
 */
void
init_pinerc(struct pine *ps, char **debug_out)
{
    char      buf[MAXPATH+1], *p, *db;
#if defined(DOS) || defined(OS2)
    char      buf2[MAXPATH+1], l_pinerc[MAXPATH+1];
    int nopinerc = 0, confregset = -1;
    register struct variable *vars = ps->vars;
#endif

#ifdef DEBUG
    /*
     * Since this routine is called before we've had a chance to set up
     * the debug file for output, we put the debugging into memory and
     * pass it back to the caller for use after init_debug(). We just
     * allocate plenty of space.
     */
    if(debug_out){
	db = *debug_out = (char *)fs_get(DSIZE * sizeof(char));
	db[0] = '\0';
    }
#endif

    mprint(2, (db, DSIZE-(db-(*debug_out)), "\n -- init_pinerc --\n\n"));

#if defined(DOS) || defined(OS2)
    /*
     * Rules for the config/support file locations under DOS are:
     *
     * 1) The location of the PINERC is searched for in the following
     *    order of precedence:
     *	     - File pointed to by '-p' command line option
     *       - File pointed to by PINERC environment variable
     *       - $HOME\pine
     *       - same dir as argv[0]
     *
     * 2) The HOME environment variable, if not set, defaults to 
     *    root of the current working drive (see alpine.c)
     * 
     * 3) The default for external files (PINE.SIG and ADDRBOOK) is the
     *    same directory as the pinerc
     *
     * 4) The support files (PINE.HLP and PINE.NDX) are expected to be in
     *    the same directory as PINE.EXE.
     */

    if(ps->prc){
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Personal config \"%.100s\" comes from command line\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
    }
    else{
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Personal config not set on cmdline, checking for $PINERC\n"));
    }

    /*
     * First, if prc hasn't been set by a command-line -p, check to see
     * if PINERC is in the environment. If so, treat it just like we
     * would have treated it if it were a command-line arg.
     */
    if(!ps->prc && (p = getenv("PINERC")) && *p){
	char path[MAXPATH], dir[MAXPATH];

	if(IS_REMOTE(p) || is_absolute_path(p)){
	    strncpy(path, p, sizeof(path)-1);
	    path[sizeof(path)-1] = '\0';
	}
	else{
	    getcwd(dir, sizeof(dir));
	    build_path(path, dir, p, sizeof(path));
	}

	if(!IS_REMOTE(p))
	  ps->pinerc = cpystr(path);

	ps->prc = new_pinerc_s(path);

	if(ps->prc){
	  mprint(2, (db, DSIZE-(db-(*debug_out)),
	  "  yes, personal config \"%.100s\" comes from $PINERC\n",
		 (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
	}
    }

    /*
     * Pinerc used to be the name of the pinerc file. Then we added
     * the possibility of the pinerc file being remote, and we replaced
     * the variable pinerc with the structure prc. Unfortunately, some
     * parts of pine rely on the fact that pinerc is the name of the
     * pinerc _file_, and use the directory that the pinerc file is located
     * in for their own purposes. We want to preserve that so things will
     * keep working. So, even if the real pinerc is remote, we need to
     * put the name of a pinerc file in the pinerc variable so that the
     * directory which contains that file is writable. The file itself
     * doesn't have to exist for this purpose, since we are really only
     * using the name of the directory containing the file. Twisted.
     * (Alternatively, we could fix all of the code that uses the pinerc
     * variable for this purpose to use a new variable which really is
     * just a directory.) hubert 2000-sep
     *
     * There are 3 cases. If pinerc is already set that means that the user
     * gave either a -p pinerc or an environment pinerc that is a local file,
     * and we are done. If pinerc is not set, then either prc is set or not.
     * If prc is set then the -p arg or PINERC value is a remote pinerc.
     * In that case we need to find a local directory to use, and put that
     * directory in the pinerc variable (with a fake filename tagged on).
     * If prc is not set, then user hasn't told us anything so we have to
     * try to find the default pinerc file by looking down the path of
     * possibilities. When we find it, we'll also use that directory.
     */
    if(!ps->pinerc){
	*l_pinerc = '\0';
	*buf = '\0';

	if(ps->prc){				/* remote pinerc case */
	    /*
	     * We don't give them an l_pinerc unless they tell us where
	     * to put it.
	     */
	    if(ps->aux_files_dir)
	      build_path(l_pinerc, ps->aux_files_dir, SYSTEM_PINERC,
			 sizeof(l_pinerc));
	    else{
		/*
		 * Search for a writable directory.
		 * Mimic what happens in !prc for local case, except we
		 * don't need to look for the actual file.
		 */

		/* check if $HOME\PINE is writable */
		build_path(buf2, ps->home_dir, DF_PINEDIR, sizeof(buf2));
		if(is_writable_dir(buf2) == 0)
		  build_path(l_pinerc, buf2, SYSTEM_PINERC, sizeof(l_pinerc));
		else{			/* $HOME\PINE not a writable dir */
		    /* use this unless registry redirects us */
		    build_path(l_pinerc, ps->pine_dir, SYSTEM_PINERC,
			       sizeof(l_pinerc));
#ifdef	_WINDOWS
		    /* if in registry, use that value */
		    if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC, buf2, sizeof(buf2))
		       && !IS_REMOTE(buf2)){
			strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
#endif
		}
	    }
	}
	else{			/* searching for pinerc file to use */
	    /*
	     * Buf2 is $HOME\PINE. If $HOME is not explicitly set,
	     * it defaults to the current working drive (often C:).
	     * See alpine.c to see how it is initially set.
	     */

	    mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, searching...\n"));
	    build_path(buf2, ps->home_dir, DF_PINEDIR, sizeof(buf2));
	    mprint(2, (db, DSIZE-(db-(*debug_out)),
	      "  checking for writable %.100s dir \"%.100s\" off of homedir\n",
		   DF_PINEDIR, buf2));
	    if(is_writable_dir(buf2) == 0){
		/*
		 * $HOME\PINE exists and is writable.
		 * See if $HOME\PINE\PINERC exists.
		 */
		build_path(buf, buf2, SYSTEM_PINERC, sizeof(buf));
		strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
		l_pinerc[sizeof(l_pinerc)-1] = '\0';
		mprint(2, (db, DSIZE-(db-(*debug_out)), "  yes, now checking for file \"%.100s\"\n",
		       buf));
		if(can_access(buf, ACCESS_EXISTS) == 0){	/* found it! */
		    /*
		     * Buf is what we were looking for.
		     * It is local and can be used for the directory, too.
		     */
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  found it\n"));
		}
		else{
		    /*
		     * No $HOME\PINE\PINERC, look for
		     * one in same dir as PINE.EXE.
		     */
		    build_path(buf2, ps->pine_dir, SYSTEM_PINERC,
			       sizeof(buf2));
		    mprint(2, (db, DSIZE-(db-(*debug_out)),
			   "  no, checking for \"%.100s\" in pine.exe dir\n",
			   buf2));
		    if(can_access(buf2, ACCESS_EXISTS) == 0){
			/* found it! */
			mprint(2, (db, DSIZE-(db-(*debug_out)), "  found it\n"));
			strncpy(buf, buf2, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
		    else{
#ifdef	_WINDOWS
			mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, checking in registry\n"));
			if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC,
				     buf2, sizeof(buf2))){
			    strncpy(buf, buf2, sizeof(buf)-1);
			    buf[sizeof(buf)-1] = '\0';
			    if(!IS_REMOTE(buf2)){
				strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
				l_pinerc[sizeof(l_pinerc)-1] = '\0';
			    }
			    /*
			     * Now buf is the pinerc to be used, l_pinerc is
			     * the directory, which may be either same as buf
			     * or it may be $HOME\PINE if registry gives us
			     * a remote pinerc.
			     */
			    mprint(2, (db, DSIZE-(db-(*debug_out)), "  found \"%.100s\" in registry\n",
				   buf));
			}
			else{
			    nopinerc = 1;
			    mprint(2, (db, DSIZE-(db-(*debug_out)), "  not found, asking user\n"));
			}
#else
			mprint(2, (db, DSIZE-(db-(*debug_out)), "  not found\n"));
#endif
		    }
		}

		/*
		 * Buf is the pinerc (could be remote if from registry)
		 * and l_pinerc is the local pinerc, which may not exist.
		 */
	    }
	    else{			/* $HOME\PINE not a writable dir */
		/*
		 * We notice that the order of checking in the registry
		 * and checking in the ALPINE.EXE directory are different
		 * in this case versus the is_writable_dir(buf2) case, and
		 * that does sort of look like a bug. However,
		 * we don't think this is a bug since we did it on purpose
		 * a long time ago. So even though we can't remember why
		 * it is this way, we think we would rediscover why if we
		 * changed it! So we won't change it.
		 */

		/*
		 * Change the default to use to the ALPINE.EXE directory.
		 */
		build_path(buf, ps->pine_dir, SYSTEM_PINERC, sizeof(buf));
		strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
		l_pinerc[sizeof(l_pinerc)-1] = '\0';
#ifdef	_WINDOWS
		mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, not writable, checking in registry\n"));
		/* if in registry, use that value */
		if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC, buf2, sizeof(buf2))){
		    strncpy(buf, buf2, sizeof(buf)-1);
		    buf[sizeof(buf)-1] = '\0';
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  found \"%.100s\" in registry\n",
			   buf));
		    if(!IS_REMOTE(buf)){
			strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
		}
		else{
		    mprint(2, (db, DSIZE-(db-(*debug_out)),
			"  no, checking for \"%.100s\" in alpine.exe dir\n",
			buf));

		    if(can_access(buf, ACCESS_EXISTS) == 0){
			mprint(2, (db, DSIZE-(db-(*debug_out)), "  found it\n"));
		    }
		    else{
			nopinerc = 1;
			mprint(2, (db, DSIZE-(db-(*debug_out)), "  not found, asking user\n"));
		    }
		}
#else
		mprint(2, (db, DSIZE-(db-(*debug_out)),
			"  no, checking for \"%.100s\" in alpine.exe dir\n",
			buf));

		if(can_access(buf, ACCESS_EXISTS) == 0){
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  found it\n"));
		}
		else{
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  not found, creating it\n"));
		}
#endif
	    }

	    /*
	     * When we get here we have buf set to the name of the
	     * pinerc, which could be local or remote. We have l_pinerc
	     * set to the same as buf if buf is local, and set to another
	     * name otherwise, hopefully contained in a writable directory.
	     */
#ifdef _WINDOWS
	    if(nopinerc || ps_global->install_flag){
		char buf3[MAXPATH+1];

		confregset = 0;
		strncpy(buf3, buf, MAXPATH);
		buf3[MAXPATH] = '\0';
		if(os_config_dialog(buf3, MAXPATH,
				    &confregset, nopinerc) == 0){
		    strncpy(buf, buf3, MAXPATH);
		    buf[MAXPATH] = '\0';
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  not found, creating it\n"));
		    mprint(2, (db, DSIZE-(db-(*debug_out)), "  user says use \"%.100s\"\n", buf));
		    if(!IS_REMOTE(buf)){
			strncpy(l_pinerc, buf, MAXPATH);
			l_pinerc[MAXPATH] = '\0';
		    }
		}
		else{
		    exit(-1);
		}
	    }
#endif
	    ps->prc = new_pinerc_s(buf);
	}

	ps->pinerc = cpystr(l_pinerc);
    }

#if defined(DOS) || defined(OS2)
    /* 
     * The goal here is to set the auxiliary directory in the pinerc variable.
     * We are making the assumption that any reference to the pinerc variable
     * after this point is used only as a directory in which to store things,
     * with the prc variable being the preferred place to store pinerc location.
     * If -aux isn't set, then there is no change. -jpf 08/2001
     */
    if(ps->aux_files_dir){
	l_pinerc[0] = '\0';
	build_path(l_pinerc, ps->aux_files_dir, SYSTEM_PINERC,
		   sizeof(l_pinerc));
	if(ps->pinerc) fs_give((void **)&ps->pinerc);
	ps->pinerc = cpystr(l_pinerc);
	mprint(2, (db, DSIZE-(db-(*debug_out)), "Setting aux_files_dir to \"%.100s\"\n",
	       ps->aux_files_dir));
    }
#endif

#ifdef	_WINDOWS
    if(confregset && (ps->update_registry != UREG_NEVER_SET))
      mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
			       || confregset == 1 ? MSWR_OP_FORCE : 0),
		MSWR_PINE_RC, 
		(ps->prc && ps->prc->name) ?
		ps->prc->name : ps->pinerc, (size_t)NULL);
#endif

    /*
     * Now that we know the default for the PINERC, build NEWSRC default.
     * Backward compatibility makes this kind of funky.  If what the
     * c-client thinks the NEWSRC should be exists *AND* it doesn't
     * already exist in the PINERC's dir, use c-client's default, otherwise
     * use the one next to the PINERC...
     */
    p = last_cmpnt(ps->pinerc);
    buf[0] = '\0';
    if(p != NULL){
	strncpy(buf, ps->pinerc, MIN(p - ps->pinerc, sizeof(buf)-1));
	buf[MIN(p - ps->pinerc, sizeof(buf)-1)] = '\0';
    }

    mprint(2, (db, DSIZE-(db-(*debug_out)), "Using directory \"%.100s\" for auxiliary files\n", buf));
    strncat(buf, "NEWSRC", sizeof(buf)-1-strlen(buf));

    if(!(p = (void *) mail_parameters(NULL, GET_NEWSRC, (void *)NULL))
       || can_access(p, ACCESS_EXISTS) < 0
       || can_access(buf, ACCESS_EXISTS) == 0){
	mail_parameters(NULL, SET_NEWSRC, (void *)buf);
	GLO_NEWSRC_PATH = cpystr(buf);
    }
    else
      GLO_NEWSRC_PATH = cpystr(p);

    if(ps->pconf){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "Global config \"%.100s\" comes from command line\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
    }
    else{
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Global config not set on cmdline, checking for $PINECONF\n"));
    }

    if(!ps->pconf && (p = getenv("PINECONF"))){
	ps->pconf = new_pinerc_s(p);
	if(ps->pconf){
	  mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "  yes, global config \"%.100s\" comes from $PINECONF\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }
#ifdef _WINDOWS
    else if(!ps->pconf
	    && mswin_reg(MSWR_OP_GET, MSWR_PINE_CONF, buf2, sizeof(buf2))){
	ps->pconf = new_pinerc_s(buf2);
	if(ps->pconf){
	    mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "  yes, global config \"%.100s\" comes from Registry\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }
#endif
    if(!ps->pconf){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, there is no global config\n"));
    }
#ifdef _WINDOWS
    else if (ps->pconf && ps->pconf->name && 
	     (ps->update_registry != UREG_NEVER_SET)){
	mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
				 ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_CONF,
		  ps->pconf->name, (size_t)NULL);
    }
#endif
    
    if(!ps->prc)
      ps->prc = new_pinerc_s(ps->pinerc);

    if(ps->exceptions){
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Exceptions config \"%.100s\" comes from command line\n",
	     ps->exceptions));
    }
    else{
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Exceptions config not set on cmdline, checking for $PINERCEX\n"));
    }

    /*
     * Exceptions is done slightly differently from pinerc. Instead of setting
     * post_prc in args.c we just set the string and use it here. We do
     * that so that we can put it in the same directory as the pinerc if
     * exceptions is a relative name, and pinerc may not be set until here.
     *
     * First, just like for pinerc, check environment variable if it wasn't
     * set on the command line.
     */
    if(!ps->exceptions && (p = getenv("PINERCEX")) && *p){
	ps->exceptions = cpystr(p);
	if(ps->exceptions){
	  mprint(2, (db, DSIZE-(db-(*debug_out)),
		 "  yes, exceptions config \"%.100s\" comes from $PINERCEX\n",
		 ps->exceptions));
	}
    }

    /*
     * If still not set, try specific file in same dir as pinerc.
     * Only use it if the file exists.
     */
    if(!ps->exceptions){
	p = last_cmpnt(ps->pinerc);
	buf[0] = '\0';
	if(p != NULL){
	    strncpy(buf, ps->pinerc, MIN(p - ps->pinerc, sizeof(buf)-1));
	    buf[MIN(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	}

	strncat(buf, "PINERCEX", sizeof(buf)-1-strlen(buf));

	mprint(2, (db, DSIZE-(db-(*debug_out)),
	       "  no, checking for default \"%.100s\" in pinerc dir\n", buf));
	if(can_access(buf, ACCESS_EXISTS) == 0)		/* found it! */
	  ps->exceptions = cpystr(buf);

	if(ps->exceptions){
	  mprint(2, (db, DSIZE-(db-(*debug_out)),
		 "  yes, exceptions config \"%.100s\" comes from default\n",
		 ps->exceptions));
	}
	else{
	  mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, there is no exceptions config\n"));
	}
    }

#else /* unix */

    if(ps->pconf){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "Global config \"%.100s\" comes from command line\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
    }

    if(!ps->pconf){
	ps->pconf = new_pinerc_s(SYSTEM_PINERC);
	if(ps->pconf){
	  mprint(2, (db, DSIZE-(db-(*debug_out)), "Global config \"%.100s\" is default\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }

    if(!ps->pconf){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "No global config!\n"));
    }

    if(ps->prc){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "Personal config \"%.100s\" comes from command line\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
    }

    if(!ps->pinerc){
      build_path(buf, ps->home_dir, ".pinerc", sizeof(buf));
      ps->pinerc = cpystr(buf);
    }

    if(!ps->prc){
	ps->prc = new_pinerc_s(ps->pinerc);
	if(ps->prc){
	  mprint(2, (db, DSIZE-(db-(*debug_out)), "Personal config \"%.100s\" is default\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
	}
    }

    if(!ps->prc){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "No personal config!\n"));
    }

    if(ps->exceptions){
      mprint(2, (db, DSIZE-(db-(*debug_out)),
	     "Exceptions config \"%.100s\" comes from command line\n",
	     ps->exceptions));
    }

    /*
     * If not set, try specific file in same dir as pinerc.
     * Only use it if the file exists.
     */
    if(!ps->exceptions){
	p = last_cmpnt(ps->pinerc);
	buf[0] = '\0';
	if(p != NULL){
	    strncpy(buf, ps->pinerc, MIN(p - ps->pinerc, sizeof(buf)-1));
	    buf[MIN(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	}

	strncat(buf, ".pinercex", sizeof(buf)-1-strlen(buf));
        mprint(2, (db, DSIZE-(db-(*debug_out)), "Exceptions config not set on cmdline\n  checking for default \"%.100s\" in pinerc dir\n", buf));

	if(can_access(buf, ACCESS_EXISTS) == 0)		/* found it! */
	  ps->exceptions = cpystr(buf);

	if(ps->exceptions){
	  mprint(2, (db, DSIZE-(db-(*debug_out)),
		 "  yes, exceptions config \"%.100s\" is default\n",
		 ps->exceptions));
	}
	else{
	  mprint(2, (db, DSIZE-(db-(*debug_out)), "  no, there is no exceptions config\n"));
	}
    }

#endif /* unix */

    if(ps->exceptions){

	if(!IS_REMOTE(ps->exceptions) &&
	   !is_absolute_path(ps->exceptions)){
#if defined(DOS) || defined(OS2)
	    p = last_cmpnt(ps->pinerc);
	    buf[0] = '\0';
	    if(p != NULL){
		strncpy(buf, ps->pinerc, MIN(p - ps->pinerc, sizeof(buf)-1));
		buf[MIN(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	    }

	    strncat(buf, ps->exceptions, sizeof(buf)-1-strlen(buf));
#else
	    build_path(buf, ps->home_dir, ps->exceptions, sizeof(buf));
#endif
	}
	else{
	    strncpy(buf, ps->exceptions, sizeof(buf)-1);
	    buf[sizeof(buf)-1] = '\0';
	}

	ps->post_prc = new_pinerc_s(buf);

	fs_give((void **)&ps->exceptions);
    }

    mprint(2, (db, DSIZE-(db-(*debug_out)), "\n  Global config:     %.100s\n",
	   (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<none>"));
    mprint(2, (db, DSIZE-(db-(*debug_out)), "  Personal config:   %.100s\n",
	   (ps->prc && ps->prc->name) ? ps->prc->name : "<none>"));
    mprint(2, (db, DSIZE-(db-(*debug_out)), "  Exceptions config: %.100s\n",
	   (ps->post_prc && ps->post_prc->name) ? ps->post_prc->name
						: "<none>"));
#if !defined(DOS) && !defined(OS2)
    if(SYSTEM_PINERC_FIXED){
      mprint(2, (db, DSIZE-(db-(*debug_out)), "  Fixed config:      %.100s\n", SYSTEM_PINERC_FIXED));
    }
#endif

    mprint(2, (db, DSIZE-(db-(*debug_out)), "\n"));
}
    

/*----------------------------------------------------------------------
     Initialize the variables

 Args:   ps   -- The usual pine structure

 Result: 

  This reads the system pine configuration file and the user's pine
configuration file ".pinerc" and places the results in the variables 
structure.  It sorts out what was read and sets a few other variables 
based on the contents.
  ----*/
void 
init_vars(struct pine *ps, void (*cmds_f) (struct pine *, char **))
{
    char	 buf[MAXPATH+1], *p, *q, **s;
    register struct variable *vars = ps->vars;
    int		 obs_header_in_reply = 0,     /* the obs_ variables are to       */
		 obs_old_style_reply = 0,     /* support backwards compatibility */
		 obs_save_by_sender, i, def_sort_rev;
    long         rvl;
    PINERC_S    *fixedprc = NULL;
    FeatureLevel obs_feature_level;
    char        *fromcharset = NULL;
    char        *err = NULL;

    dprint((5, "init_vars:\n"));

    /*--- The defaults here are defined in os-xxx.h so they can vary
          per machine ---*/

    GLO_PRINTER			= cpystr(DF_DEFAULT_PRINTER);
    GLO_ELM_STYLE_SAVE		= cpystr(DF_ELM_STYLE_SAVE);
    GLO_SAVE_BY_SENDER		= cpystr(DF_SAVE_BY_SENDER);
    GLO_HEADER_IN_REPLY		= cpystr(DF_HEADER_IN_REPLY);
    GLO_INBOX_PATH		= cpystr("inbox");
    GLO_DEFAULT_FCC		= cpystr(DF_DEFAULT_FCC);
    GLO_DEFAULT_SAVE_FOLDER	= cpystr(DEFAULT_SAVE);
    GLO_POSTPONED_FOLDER	= cpystr(POSTPONED_MSGS);
    GLO_USE_ONLY_DOMAIN_NAME	= cpystr(DF_USE_ONLY_DOMAIN_NAME);
    GLO_FEATURE_LEVEL		= cpystr("sappling");
    GLO_OLD_STYLE_REPLY		= cpystr(DF_OLD_STYLE_REPLY);
    GLO_SORT_KEY		= cpystr(DF_SORT_KEY);
    GLO_SAVED_MSG_NAME_RULE	= cpystr(DF_SAVED_MSG_NAME_RULE);
    GLO_FCC_RULE		= cpystr(DF_FCC_RULE);
    GLO_AB_SORT_RULE		= cpystr(DF_AB_SORT_RULE);
    GLO_FLD_SORT_RULE		= cpystr(DF_FLD_SORT_RULE);
    GLO_SIGNATURE_FILE		= cpystr(DF_SIGNATURE_FILE);
    GLO_MAIL_DIRECTORY		= cpystr(DF_MAIL_DIRECTORY);
    GLO_REMOTE_ABOOK_HISTORY	= cpystr(DF_REMOTE_ABOOK_HISTORY);
    GLO_REMOTE_ABOOK_VALIDITY	= cpystr(DF_REMOTE_ABOOK_VALIDITY);
    GLO_GOTO_DEFAULT_RULE	= cpystr(DF_GOTO_DEFAULT_RULE);
    GLO_INCOMING_STARTUP	= cpystr(DF_INCOMING_STARTUP);
    GLO_PRUNING_RULE		= cpystr(DF_PRUNING_RULE);
    GLO_REOPEN_RULE		= cpystr(DF_REOPEN_RULE);
    GLO_THREAD_DISP_STYLE	= cpystr(DF_THREAD_DISP_STYLE);
    GLO_THREAD_INDEX_STYLE	= cpystr(DF_THREAD_INDEX_STYLE);
    GLO_THREAD_MORE_CHAR	= cpystr(DF_THREAD_MORE_CHAR);
    GLO_THREAD_EXP_CHAR		= cpystr(DF_THREAD_EXP_CHAR);
    GLO_THREAD_LASTREPLY_CHAR	= cpystr(DF_THREAD_LASTREPLY_CHAR);
    GLO_BUGS_FULLNAME		= cpystr("Sorry No Address");
    GLO_BUGS_ADDRESS		= cpystr("nobody");
    GLO_SUGGEST_FULLNAME	= cpystr("Sorry No Address");
    GLO_SUGGEST_ADDRESS		= cpystr("nobody");
    GLO_LOCAL_FULLNAME		= cpystr(DF_LOCAL_FULLNAME);
    GLO_LOCAL_ADDRESS		= cpystr(DF_LOCAL_ADDRESS);
    GLO_OVERLAP			= cpystr(DF_OVERLAP);
    GLO_MAXREMSTREAM		= cpystr(DF_MAXREMSTREAM);
    GLO_MARGIN			= cpystr(DF_MARGIN);
    GLO_FILLCOL			= cpystr(DF_FILLCOL);
    GLO_DEADLETS		= cpystr(DF_DEADLETS);
    GLO_NMW_WIDTH		= cpystr(DF_NMW_WIDTH);
    GLO_REPLY_STRING		= cpystr("> ");
    GLO_REPLY_INTRO		= cpystr(DEFAULT_REPLY_INTRO);
    GLO_EMPTY_HDR_MSG		= cpystr("undisclosed-recipients");
    GLO_STATUS_MSG_DELAY	= cpystr("0");
    GLO_ACTIVE_MSG_INTERVAL	= cpystr("12");
    GLO_USERINPUTTIMEO		= cpystr("0");
    GLO_INCCHECKTIMEO		= cpystr("5");
    GLO_INCCHECKINTERVAL	= cpystr("180");
    GLO_MAILCHECK		= cpystr(DF_MAILCHECK);
    GLO_MAILCHECKNONCURR	= cpystr("0");
    GLO_MAILDROPCHECK		= cpystr(DF_MAILDROPCHECK);
    GLO_NNTPRANGE		= cpystr("0");
    GLO_KBLOCK_PASSWD_COUNT	= cpystr(DF_KBLOCK_PASSWD_COUNT);
    GLO_INDEX_COLOR_STYLE	= cpystr("flip-colors");
    GLO_TITLEBAR_COLOR_STYLE	= cpystr("default");
    GLO_POST_CHAR_SET		= cpystr("UTF-8");
#ifdef	DF_FOLDER_EXTENSION
    GLO_FOLDER_EXTENSION	= cpystr(DF_FOLDER_EXTENSION);
#endif
#ifdef	DF_SMTP_SERVER
    GLO_SMTP_SERVER		= parse_list(DF_SMTP_SERVER, 1,
					     PL_REMSURRQUOT, NULL);
#endif
#ifndef	_WINDOWS
    GLO_COLOR_STYLE		= cpystr("no-color");
    GLO_NORM_FORE_COLOR		= cpystr(DEFAULT_NORM_FORE_RGB);
    GLO_NORM_BACK_COLOR		= cpystr(DEFAULT_NORM_BACK_RGB);
#endif
    GLO_TITLE_FORE_COLOR	= cpystr(DEFAULT_TITLE_FORE_RGB);
    GLO_TITLE_BACK_COLOR	= cpystr(DEFAULT_TITLE_BACK_RGB);
    GLO_TITLECLOSED_FORE_COLOR	= cpystr(DEFAULT_TITLECLOSED_FORE_RGB);
    GLO_TITLECLOSED_BACK_COLOR	= cpystr(DEFAULT_TITLECLOSED_BACK_RGB);
    GLO_METAMSG_FORE_COLOR	= cpystr(DEFAULT_METAMSG_FORE_RGB);
    GLO_METAMSG_BACK_COLOR	= cpystr(DEFAULT_METAMSG_BACK_RGB);
    GLO_QUOTE1_FORE_COLOR	= cpystr(DEFAULT_QUOTE1_FORE_RGB);
    GLO_QUOTE1_BACK_COLOR	= cpystr(DEFAULT_QUOTE1_BACK_RGB);
    GLO_QUOTE2_FORE_COLOR	= cpystr(DEFAULT_QUOTE2_FORE_RGB);
    GLO_QUOTE2_BACK_COLOR	= cpystr(DEFAULT_QUOTE2_BACK_RGB);
    GLO_QUOTE3_FORE_COLOR	= cpystr(DEFAULT_QUOTE3_FORE_RGB);
    GLO_QUOTE3_BACK_COLOR	= cpystr(DEFAULT_QUOTE3_BACK_RGB);
    GLO_SIGNATURE_FORE_COLOR	= cpystr(DEFAULT_SIGNATURE_FORE_RGB);
    GLO_SIGNATURE_BACK_COLOR	= cpystr(DEFAULT_SIGNATURE_BACK_RGB);
    GLO_IND_PLUS_FORE_COLOR	= cpystr(DEFAULT_IND_PLUS_FORE_RGB);
    GLO_IND_PLUS_BACK_COLOR	= cpystr(DEFAULT_IND_PLUS_BACK_RGB);
    GLO_IND_IMP_FORE_COLOR	= cpystr(DEFAULT_IND_IMP_FORE_RGB);
    GLO_IND_IMP_BACK_COLOR	= cpystr(DEFAULT_IND_IMP_BACK_RGB);
    GLO_IND_ANS_FORE_COLOR	= cpystr(DEFAULT_IND_ANS_FORE_RGB);
    GLO_IND_ANS_BACK_COLOR	= cpystr(DEFAULT_IND_ANS_BACK_RGB);
    GLO_IND_NEW_FORE_COLOR	= cpystr(DEFAULT_IND_NEW_FORE_RGB);
    GLO_IND_NEW_BACK_COLOR	= cpystr(DEFAULT_IND_NEW_BACK_RGB);
    GLO_IND_OP_FORE_COLOR	= cpystr(DEFAULT_IND_OP_FORE_RGB);
    GLO_IND_OP_BACK_COLOR	= cpystr(DEFAULT_IND_OP_BACK_RGB);
    if(!GLO_VIEW_HDR_COLORS)
      GLO_VIEW_HDR_COLORS = parse_list(DEFAULT_VIEW_HDR_COLORS, 1, PL_REMSURRQUOT, NULL);
    GLO_VIEW_MARGIN_LEFT	= cpystr("0");
    GLO_VIEW_MARGIN_RIGHT	= cpystr(DF_VIEW_MARGIN_RIGHT);
    GLO_QUOTE_SUPPRESSION	= cpystr(DF_QUOTE_SUPPRESSION);
    GLO_KW_BRACES		= cpystr("\"{\" \"} \"");
    GLO_WP_INDEXHEIGHT          = cpystr("24");
    GLO_WP_AGGSTATE		= cpystr("1");
    GLO_WP_STATE		= cpystr("");

    /*
     * Default first value for addrbook list if none set.
     * We also want to be sure to set global_val to the default
     * if is_fixed, so that address-book= will cause the default to happen.
     */
    if(!GLO_ADDRESSBOOK && !FIX_ADDRESSBOOK)
      GLO_ADDRESSBOOK = parse_list(DF_ADDRESSBOOK, 1, 0, NULL);

    /*
     * Default first value if none set.
     */
    if(!GLO_STANDARD_PRINTER && !FIX_STANDARD_PRINTER)
      GLO_STANDARD_PRINTER = parse_list(DF_STANDARD_PRINTER, 1, 0, NULL);

#if !defined(DOS) && !defined(OS2)
    /*
     * This is here instead of in init_pinerc so that we can get by without
     * having a global fixedprc, since we don't need it anymore after this.
     */
    fixedprc = new_pinerc_s(SYSTEM_PINERC_FIXED);
#endif

    if(ps->pconf){
	read_pinerc(ps->pconf, vars, ParseGlobal);
	if(ps->pconf->type != Loc)
	  rd_close_remote(ps->pconf->rd);
    }

    if(ps->prc){
	read_pinerc(ps->prc, vars, ParsePers);
	if(ps->prc->type != Loc)
	  rd_close_remote(ps->prc->rd);
    }

    if(ps->post_prc){
	read_pinerc(ps->post_prc, vars, ParsePersPost);
	if(ps->post_prc->type != Loc)
	  rd_close_remote(ps->post_prc->rd);
    }

    if(fixedprc){
	read_pinerc(fixedprc, vars, ParseFixed);
	free_pinerc_s(&fixedprc);
    }

    ps->ew_for_except_vars = ps->post_prc ? Post : Main;

    if(ps->exit_if_no_pinerc && ps->first_time_user){

	/* TRANSLATORS: -bail is a literal option name, don't change it. */
	exceptional_exit(_("Exiting because -bail option is set and config file doesn't exist."), -1);
    }

    /*
     * Convert everything having to do with the config to UTF-8
     * in order to avoid having to worry about it all over the
     * place.
     * Set the character-set first so that we may use that in
     * the conversion process.
     */
    set_collation(0, 1);

#if (HAVE_LANGINFO_H && defined(CODESET))

    if(output_charset_is_supported(nl_langinfo(CODESET)))
      ps->GLO_CHAR_SET = cpystr(nl_langinfo(CODESET));
    else{
	ps->GLO_CHAR_SET = cpystr("UTF-8");
        dprint((1,"nl_langinfo(CODESET) returns unrecognized value=\"%s\", using UTF-8 as default\n", (p=nl_langinfo(CODESET)) ? p : ""));
    }
#else
    ps->GLO_CHAR_SET = cpystr("UTF-8");
#endif

    set_current_val(&vars[V_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_OLD_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_POST_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_KEY_CHAR_SET], TRUE, TRUE);

    /*
     * Also set up the feature list because we need the
     * Use-System-Translation feature to set up the charmaps.
     */

    /* way obsolete, backwards compatibility */
    set_current_val(&vars[V_FEATURE_LEVEL], TRUE, TRUE);
    if(strucmp(VAR_FEATURE_LEVEL, "seedling") == 0)
      obs_feature_level = Seedling;
    else if(strucmp(VAR_FEATURE_LEVEL, "old-growth") == 0)
      obs_feature_level = Seasoned;
    else
      obs_feature_level = Sapling;

    /* obsolete, backwards compatibility */
    set_current_val(&vars[V_OLD_STYLE_REPLY], TRUE, TRUE);
    obs_old_style_reply = !strucmp(VAR_OLD_STYLE_REPLY, "yes");

    set_feature_list_current_val(&vars[V_FEATURE_LIST]);
    process_feature_list(ps, VAR_FEATURE_LIST,
           (obs_feature_level == Seasoned) ? 1 : 0,
	   obs_header_in_reply, obs_old_style_reply);


    /*
     * Redo set_collation call with correct value for collation,
     * but we're hardwiring ctype on now. That's because nl_langinfo()
     * call needs it and system-dependent wcwidth and wcrtomb functions
     * need it.
     */
    set_collation(F_OFF(F_DISABLE_SETLOCALE_COLLATE, ps_global), 1);

    /*
     * Set up to send the correct sequence of bytes to the display terminal.
     */

    if(reset_character_set_stuff(&err) == -1)
      panic(err ? err : "trouble with character set setup");
    else if(err){
	init_error(ps, SM_ORDER | SM_DING, 3, 5, err);
	fs_give((void **) &err);
    }

    /*
     * Now we use the configvars from above to convert the rest
     * to UTF-8. That should be ok because the ones above should
     * be ASCII.
     */
    if(ps->keyboard_charmap && strucmp(ps->keyboard_charmap, "UTF-8")
       &&  strucmp(ps->keyboard_charmap, "US-ASCII"))
      fromcharset = ps->keyboard_charmap;
    else if(ps->display_charmap && strucmp(ps->display_charmap, "UTF-8")
       &&  strucmp(ps->display_charmap, "US-ASCII"))
      fromcharset = ps->display_charmap;
    else if(VAR_OLD_CHAR_SET && strucmp(VAR_OLD_CHAR_SET, "UTF-8")
       &&  strucmp(VAR_OLD_CHAR_SET, "US-ASCII"))
      fromcharset = VAR_OLD_CHAR_SET;

    convert_configvars_to_utf8(vars, fromcharset);

    /*
     * If we already set this while reading the remote pinerc, don't
     * change it.
     */
    if(!VAR_REMOTE_ABOOK_METADATA || !VAR_REMOTE_ABOOK_METADATA[0])
      set_current_val(&vars[V_REMOTE_ABOOK_METADATA], TRUE, TRUE);

    /*
     * mail-directory variable is obsolete, put its value in
     * default folder-collection list
     */
    set_current_val(&vars[V_MAIL_DIRECTORY], TRUE, TRUE);
    if(!GLO_FOLDER_SPEC){
	build_path(tmp_20k_buf, VAR_MAIL_DIRECTORY, "[]", SIZEOF_20KBUF);
	GLO_FOLDER_SPEC = parse_list(tmp_20k_buf, 1, 0, NULL);
    }

    set_current_val(&vars[V_FOLDER_SPEC], TRUE, TRUE);

    set_current_val(&vars[V_NNTP_SERVER], TRUE, TRUE);
    for(i = 0; VAR_NNTP_SERVER && VAR_NNTP_SERVER[i]; i++)
      removing_quotes(VAR_NNTP_SERVER[i]);

    set_news_spec_current_val(TRUE, TRUE);

    set_current_val(&vars[V_INBOX_PATH], TRUE, TRUE);

    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
    if(VAR_USER_DOMAIN
       && VAR_USER_DOMAIN[0]
       && (p = strrindex(VAR_USER_DOMAIN, '@'))){
	if(*(++p)){
	    char *q;

	    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		    "User-domain (%s) cannot contain \"@\", using \"%s\"",
		    VAR_USER_DOMAIN, p);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    q = VAR_USER_DOMAIN;
	    while((*q++ = *p++) != '\0')
	      ;/* do nothing */
	}
	else{
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		    "User-domain (%s) cannot contain \"@\", deleting",
		    VAR_USER_DOMAIN);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    if(ps->vars[V_USER_DOMAIN].post_user_val.p){
		fs_give((void **)&ps->vars[V_USER_DOMAIN].post_user_val.p);
		set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
	    }

	    if(VAR_USER_DOMAIN
	       && VAR_USER_DOMAIN[0]
	       && (p = strrindex(VAR_USER_DOMAIN, '@'))){
		if(ps->vars[V_USER_DOMAIN].main_user_val.p){
		    fs_give((void **)&ps->vars[V_USER_DOMAIN].main_user_val.p);
		    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		}
	    }
	}
    }

    set_current_val(&vars[V_USE_ONLY_DOMAIN_NAME], TRUE, TRUE);
    set_current_val(&vars[V_REPLY_STRING], TRUE, TRUE);
    set_current_val(&vars[V_WORDSEPS], TRUE, TRUE);
    set_current_val(&vars[V_QUOTE_REPLACE_STRING], TRUE, TRUE);
    set_current_val(&vars[V_REPLY_INTRO], TRUE, TRUE);
    set_current_val(&vars[V_EMPTY_HDR_MSG], TRUE, TRUE);

#ifdef	ENABLE_LDAP
    set_current_val(&vars[V_LDAP_SERVERS], TRUE, TRUE);
#endif	/* ENABLE_LDAP */

    /* obsolete, backwards compatibility */
    set_current_val(&vars[V_HEADER_IN_REPLY], TRUE, TRUE);
    obs_header_in_reply=!strucmp(VAR_HEADER_IN_REPLY, "yes");

    set_current_val(&vars[V_PERSONAL_PRINT_COMMAND], TRUE, TRUE);
    set_current_val(&vars[V_STANDARD_PRINTER], TRUE, TRUE);
    set_current_val(&vars[V_PRINTER], TRUE, TRUE);
    /*
     * We don't want the user to be able to edit their pinerc and set
     * printer to whatever they want if personal-print-command is fixed.
     * So make sure printer is set to something legitimate.
     */
    if(vars[V_PERSONAL_PRINT_COMMAND].is_fixed && !vars[V_PRINTER].is_fixed){
	char **tt;
	char   aname[100], wname[100];
	int    ok = 0;

	strncat(strncpy(aname, ANSI_PRINTER, 60), "-no-formfeed", 30);
	strncat(strncpy(wname, WYSE_PRINTER, 60), "-no-formfeed", 30);
	if(strucmp(VAR_PRINTER, ANSI_PRINTER) == 0
	  || strucmp(VAR_PRINTER, aname) == 0
	  || strucmp(VAR_PRINTER, WYSE_PRINTER) == 0
	  || strucmp(VAR_PRINTER, wname) == 0)
	  ok++;
	else if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0]){
	    for(tt = VAR_STANDARD_PRINTER; *tt; tt++)
	      if(strucmp(VAR_PRINTER, *tt) == 0)
		break;
	    
	    if(*tt)
	      ok++;
	}

	if(!ok){
	    char            *val;
	    struct variable *v;

	    if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0])
	      val = VAR_STANDARD_PRINTER[0];
	    else
	      val = ANSI_PRINTER;
	    
	    v = &vars[V_PRINTER];
	    if(v->main_user_val.p)
	      fs_give((void **)&v->main_user_val.p);
	    if(v->post_user_val.p)
	      fs_give((void **)&v->post_user_val.p);
	    if(v->current_val.p)
	      fs_give((void **)&v->current_val.p);
	    
	    v->main_user_val.p = cpystr(val);
	    v->current_val.p = cpystr(val);
	}
    }

    set_current_val(&vars[V_LAST_TIME_PRUNE_QUESTION], TRUE, TRUE);
    if(VAR_LAST_TIME_PRUNE_QUESTION != NULL){
        /* The month value in the file runs from 1-12, the variable here
           runs from 0-11; the value in the file used to be 0-11, but we're 
           fixing it in January */
        ps->last_expire_year  = atoi(VAR_LAST_TIME_PRUNE_QUESTION);
        ps->last_expire_month =
			atoi(strindex(VAR_LAST_TIME_PRUNE_QUESTION, '.') + 1);
        if(ps->last_expire_month == 0){
            /* Fix for 0 because of old bug */
            snprintf(buf, sizeof(buf), "%d.%d", ps_global->last_expire_year,
              ps_global->last_expire_month + 1);
            set_variable(V_LAST_TIME_PRUNE_QUESTION, buf, 1, 1, Main);
        }else{
            ps->last_expire_month--; 
        } 
    }else{
        ps->last_expire_year  = -1;
        ps->last_expire_month = -1;
    }

    set_current_val(&vars[V_BUGS_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_BUGS_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_SUGGEST_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_SUGGEST_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_LOCAL_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_LOCAL_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_BUGS_EXTRAS], TRUE, TRUE);
    set_current_val(&vars[V_KBLOCK_PASSWD_COUNT], TRUE, TRUE);
    set_current_val(&vars[V_DEFAULT_FCC], TRUE, TRUE);
    set_current_val(&vars[V_POSTPONED_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_READ_MESSAGE_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_FORM_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_EDITOR], TRUE, TRUE);
    set_current_val(&vars[V_SPELLER], TRUE, TRUE);
    set_current_val(&vars[V_IMAGE_VIEWER], TRUE, TRUE);
    set_current_val(&vars[V_BROWSER], TRUE, TRUE);
    set_current_val(&vars[V_SMTP_SERVER], TRUE, TRUE);
    set_current_val(&vars[V_COMP_HDRS], TRUE, TRUE);
    set_current_val(&vars[V_CUSTOM_HDRS], TRUE, TRUE);
    set_current_val(&vars[V_SENDMAIL_PATH], TRUE, TRUE);
    set_current_val(&vars[V_DISPLAY_FILTERS], TRUE, TRUE);
    set_current_val(&vars[V_SEND_FILTER], TRUE, TRUE);
    set_current_val(&vars[V_ALT_ADDRS], TRUE, TRUE);
    set_current_val(&vars[V_ABOOK_FORMATS], TRUE, TRUE);
    set_current_val(&vars[V_KW_BRACES], TRUE, TRUE);

    set_current_val(&vars[V_KEYWORDS], TRUE, TRUE);
    ps_global->keywords = init_keyword_list(VAR_KEYWORDS);

    set_current_val(&vars[V_OPER_DIR], TRUE, TRUE);
    if(VAR_OPER_DIR && !VAR_OPER_DIR[0]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
 "Setting operating-dir to the empty string is not allowed.  Will be ignored.");
	fs_give((void **)&VAR_OPER_DIR);
	if(FIX_OPER_DIR)
	  fs_give((void **)&FIX_OPER_DIR);
	if(GLO_OPER_DIR)
	  fs_give((void **)&GLO_OPER_DIR);
	if(COM_OPER_DIR)
	  fs_give((void **)&COM_OPER_DIR);
	if(ps_global->vars[V_OPER_DIR].post_user_val.p)
	  fs_give((void **)&ps_global->vars[V_OPER_DIR].post_user_val.p);
	if(ps_global->vars[V_OPER_DIR].main_user_val.p)
	  fs_give((void **)&ps_global->vars[V_OPER_DIR].main_user_val.p);
    }

    set_current_val(&vars[V_PERSONAL_PRINT_CATEGORY], TRUE, TRUE);
    ps->printer_category = -1;
    if(VAR_PERSONAL_PRINT_CATEGORY != NULL)
      ps->printer_category = atoi(VAR_PERSONAL_PRINT_CATEGORY);

    if(ps->printer_category < 1 || ps->printer_category > 3){
	char **tt;
	char aname[100], wname[100];

	strncat(strncpy(aname, ANSI_PRINTER, 60), "-no-formfeed", 30);
	strncat(strncpy(wname, WYSE_PRINTER, 60), "-no-formfeed", 30);
	if(strucmp(VAR_PRINTER, ANSI_PRINTER) == 0
	  || strucmp(VAR_PRINTER, aname) == 0
	  || strucmp(VAR_PRINTER, WYSE_PRINTER) == 0
	  || strucmp(VAR_PRINTER, wname) == 0)
	  ps->printer_category = 1;
	else if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0]){
	    for(tt = VAR_STANDARD_PRINTER; *tt; tt++)
	      if(strucmp(VAR_PRINTER, *tt) == 0)
		break;
	    
	    if(*tt)
	      ps->printer_category = 2;
	}

	/* didn't find it yet */
	if(ps->printer_category < 1 || ps->printer_category > 3){
	    if(VAR_PERSONAL_PRINT_COMMAND && VAR_PERSONAL_PRINT_COMMAND[0]){
		for(tt = VAR_PERSONAL_PRINT_COMMAND; *tt; tt++)
		  if(strucmp(VAR_PRINTER, *tt) == 0)
		    break;
		
		if(*tt)
		  ps->printer_category = 3;
	    }
	}
    }

    set_current_val(&vars[V_OVERLAP], TRUE, TRUE);
    ps->viewer_overlap = i = atoi(DF_OVERLAP);
    if(SVAR_OVERLAP(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->viewer_overlap = i;

    set_current_val(&vars[V_MARGIN], TRUE, TRUE);
    ps->scroll_margin = i = atoi(DF_MARGIN);
    if(SVAR_MARGIN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->scroll_margin = i;

    set_current_val(&vars[V_FILLCOL], TRUE, TRUE);
    ps->composer_fillcol = i = atoi(DF_FILLCOL);
    if(SVAR_FILLCOL(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->composer_fillcol = i;

    set_current_val(&vars[V_QUOTE_SUPPRESSION], TRUE, TRUE);
    ps->quote_suppression_threshold = i = atoi(DF_QUOTE_SUPPRESSION);
    if(SVAR_QUOTE_SUPPRESSION(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else{
	if(i > 0 && i < Q_SUPP_LIMIT){
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		"Ignoring Quote-Suppression-Threshold value of %.50s, see help",
		VAR_QUOTE_SUPPRESSION);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
	else{
	    if(i < 0 && i != Q_DEL_ALL)
	      ps->quote_suppression_threshold = -i;
	    else
	      ps->quote_suppression_threshold = i;
	}
    }
    
    set_current_val(&vars[V_DEADLETS], TRUE, TRUE);
    ps->deadlets = i = atoi(DF_DEADLETS);
    if(SVAR_DEADLETS(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->deadlets = i;
    
    set_current_val(&vars[V_STATUS_MSG_DELAY], TRUE, TRUE);
    ps->status_msg_delay = i = 0;
    if(SVAR_MSGDLAY(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->status_msg_delay = i;

    set_current_val(&vars[V_ACTIVE_MSG_INTERVAL], TRUE, TRUE);
    ps->active_status_interval = i = 8;
    if(SVAR_ACTIVEINTERVAL(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->active_status_interval = i;

    set_current_val(&vars[V_REMOTE_ABOOK_HISTORY], TRUE, TRUE);
    ps->remote_abook_history = i = atoi(DF_REMOTE_ABOOK_HISTORY);
    if(SVAR_AB_HIST(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->remote_abook_history = i;

    set_current_val(&vars[V_REMOTE_ABOOK_VALIDITY], TRUE, TRUE);
    ps->remote_abook_validity = i = atoi(DF_REMOTE_ABOOK_VALIDITY);
    if(SVAR_AB_VALID(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->remote_abook_validity = i;

    set_current_val(&vars[V_USERINPUTTIMEO], TRUE, TRUE);
    ps->hours_to_timeout = i = 0;
    if(SVAR_USER_INPUT(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->hours_to_timeout = i;

    /* timeo is a regular extern int because it is referenced in pico */
    set_current_val(&vars[V_MAILCHECK], TRUE, TRUE);
    set_input_timeout(i = 15);
    if(SVAR_MAILCHK(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      set_input_timeout(i);

    set_current_val(&vars[V_MAILCHECKNONCURR], TRUE, TRUE);
    ps->check_interval_for_noncurr = i = 0;
    if(SVAR_MAILCHKNONCURR(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->check_interval_for_noncurr = i;

#ifdef DEBUGJOURNAL
    ps->debugmem = 1;
#else
    ps->debugmem = 0;
#endif

    i = 30;
    set_current_val(&vars[V_TCPOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPOPENTIMEO && SVAR_TCP_OPEN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_TCPREADWARNTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPREADWARNTIMEO && SVAR_TCP_READWARN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 0;
    set_current_val(&vars[V_TCPWRITEWARNTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPWRITEWARNTIMEO && SVAR_TCP_WRITEWARN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_RSHOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_RSHOPENTIMEO && SVAR_RSH_OPEN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_SSHOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_SSHOPENTIMEO && SVAR_SSH_OPEN(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    set_current_val(&vars[V_INCCHECKLIST], TRUE, TRUE);

    set_current_val(&vars[V_INCCHECKTIMEO], TRUE, TRUE);
    ps->inc_check_timeout = i = 5;
    if(SVAR_INC_CHECK_TIMEO(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->inc_check_timeout = i;

    set_current_val(&vars[V_INCCHECKINTERVAL], TRUE, TRUE);
    ps->inc_check_interval = i = 180;
    if(SVAR_INC_CHECK_INTERV(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->inc_check_interval = i;

    rvl = 60L;
    set_current_val(&vars[V_MAILDROPCHECK], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_MAILDROPCHECK && SVAR_MAILDCHK(ps, rvl, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    rvl = 0L;
    set_current_val(&vars[V_NNTPRANGE], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_NNTPRANGE && SVAR_NNTPRANGE(ps, rvl, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    set_current_val(&vars[V_TCPQUERYTIMEO], TRUE, TRUE);
    ps->tcp_query_timeout = i = TO_BAIL_THRESHOLD;
    if(VAR_TCPQUERYTIMEO && SVAR_TCP_QUERY(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->tcp_query_timeout = i;

    set_current_val(&vars[V_NEWSRC_PATH], TRUE, TRUE);
    if(VAR_NEWSRC_PATH && VAR_NEWSRC_PATH[0])
      mail_parameters(NULL, SET_NEWSRC, (void *)VAR_NEWSRC_PATH);

    set_current_val(&vars[V_NEWS_ACTIVE_PATH], TRUE, TRUE);
    if(VAR_NEWS_ACTIVE_PATH)
      mail_parameters(NULL, SET_NEWSACTIVE,
		      (void *)VAR_NEWS_ACTIVE_PATH);

    set_current_val(&vars[V_NEWS_SPOOL_DIR], TRUE, TRUE);
    if(VAR_NEWS_SPOOL_DIR)
      mail_parameters(NULL, SET_NEWSSPOOL,
		      (void *)VAR_NEWS_SPOOL_DIR);

    /* guarantee a save default */
    set_current_val(&vars[V_DEFAULT_SAVE_FOLDER], TRUE, TRUE);
    if(!VAR_DEFAULT_SAVE_FOLDER || !VAR_DEFAULT_SAVE_FOLDER[0])
      set_variable(V_DEFAULT_SAVE_FOLDER,
		   (GLO_DEFAULT_SAVE_FOLDER && GLO_DEFAULT_SAVE_FOLDER[0])
		     ? GLO_DEFAULT_SAVE_FOLDER
		     : DEFAULT_SAVE, 1, 0, Main);

    set_current_val(&vars[V_SIGNATURE_FILE], TRUE, TRUE);
    set_current_val(&vars[V_LITERAL_SIG], TRUE, TRUE);
    set_current_val(&vars[V_GLOB_ADDRBOOK], TRUE, TRUE);
    set_current_val(&vars[V_ADDRESSBOOK], TRUE, TRUE);
    set_current_val(&vars[V_FORCED_ABOOK_ENTRY], TRUE, TRUE);
    set_current_val(&vars[V_DISABLE_DRIVERS], TRUE, TRUE);
    set_current_val(&vars[V_DISABLE_AUTHS], TRUE, TRUE);

    set_current_val(&vars[V_VIEW_HEADERS], TRUE, TRUE);
    /* strip spaces and colons */
    if(ps->VAR_VIEW_HEADERS){
	for(s = ps->VAR_VIEW_HEADERS; (q = *s) != NULL; s++){
	    if(q[0]){
		removing_leading_white_space(q);
		/* look for colon or space or end */
		for(p = q; *p && !isspace((unsigned char)*p) && *p != ':'; p++)
		  ;/* do nothing */
		
		*p = '\0';
		if(strucmp(q, ALL_EXCEPT) == 0)
		  ps->view_all_except = 1;
	    }
	}
    }

    set_current_val(&vars[V_VIEW_MARGIN_LEFT], TRUE, TRUE);
    set_current_val(&vars[V_VIEW_MARGIN_RIGHT], TRUE, TRUE);
    set_current_val(&vars[V_UPLOAD_CMD], TRUE, TRUE);
    set_current_val(&vars[V_UPLOAD_CMD_PREFIX], TRUE, TRUE);
    set_current_val(&vars[V_DOWNLOAD_CMD], TRUE, TRUE);
    set_current_val(&vars[V_DOWNLOAD_CMD_PREFIX], TRUE, TRUE);
    set_current_val(&vars[V_MAILCAP_PATH], TRUE, TRUE);
    set_current_val(&vars[V_MIMETYPE_PATH], TRUE, TRUE);
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    set_current_val(&vars[V_FIFOPATH], TRUE, TRUE);
#endif

    set_current_val(&vars[V_RSHPATH], TRUE, TRUE);
    if(VAR_RSHPATH
       && is_absolute_path(VAR_RSHPATH)
       && can_access(VAR_RSHPATH, EXECUTE_ACCESS) == 0){
	mail_parameters(NULL, SET_RSHPATH, (void *) VAR_RSHPATH);
    }

    set_current_val(&vars[V_RSHCMD], TRUE, TRUE);
    if(VAR_RSHCMD){
	mail_parameters(NULL, SET_RSHCOMMAND, (void *) VAR_RSHCMD);
    }

    set_current_val(&vars[V_SSHPATH], TRUE, TRUE);
    if(VAR_SSHPATH
       && is_absolute_path(VAR_SSHPATH)
       && can_access(VAR_SSHPATH, EXECUTE_ACCESS) == 0){
	mail_parameters(NULL, SET_SSHPATH, (void *) VAR_SSHPATH);
    }

    set_current_val(&vars[V_SSHCMD], TRUE, TRUE);
    if(VAR_SSHCMD){
	mail_parameters(NULL, SET_SSHCOMMAND, (void *) VAR_SSHCMD);
    }

#if	defined(DOS) || defined(OS2)

    set_current_val(&vars[V_FILE_DIR], TRUE, TRUE);

#ifdef	_WINDOWS
    set_current_val(&vars[V_FONT_NAME], TRUE, TRUE);
    set_current_val(&vars[V_FONT_SIZE], TRUE, TRUE);
    set_current_val(&vars[V_FONT_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_FONT_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_CURSOR_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_WINDOW_POSITION], TRUE, TRUE);

    if(F_OFF(F_STORE_WINPOS_IN_CONFIG, ps_global)){
	/* if win position is in the registry, use it */
	if(mswin_reg(MSWR_OP_GET, MSWR_PINE_POS, buf, sizeof(buf))){
	    if(VAR_WINDOW_POSITION)
	      fs_give((void **)&VAR_WINDOW_POSITION);
	    
	    VAR_WINDOW_POSITION = cpystr(buf);
	}
	else if(VAR_WINDOW_POSITION
	    && (ps->update_registry != UREG_NEVER_SET)){
	    /* otherwise, put it there */
	    mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
				     ? MSWR_OP_FORCE : 0),
		      MSWR_PINE_POS, 
		      VAR_WINDOW_POSITION, (size_t)NULL);
	}
    }

    mswin_setwindow (VAR_FONT_NAME, VAR_FONT_SIZE, 
		     VAR_FONT_STYLE, VAR_WINDOW_POSITION,
		     VAR_CURSOR_STYLE, VAR_FONT_CHAR_SET);

    /* this is no longer used */
    if(VAR_WINDOW_POSITION)
      fs_give((void **)&VAR_WINDOW_POSITION);

    set_current_val(&vars[V_PRINT_FONT_NAME], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_SIZE], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_CHAR_SET], TRUE, TRUE);
    mswin_setprintfont (VAR_PRINT_FONT_NAME,
			VAR_PRINT_FONT_SIZE,
			VAR_PRINT_FONT_STYLE,
			VAR_PRINT_FONT_CHAR_SET);

    mswin_setgenhelptextcallback(pcpine_general_help);

    mswin_setclosetext ("Use the \"Q\" command to exit Alpine.");

    {
	char foreColor[64], backColor[64];

	mswin_getwindow(NULL, 0, NULL, 0, NULL, 0, NULL, 0,
			foreColor, sizeof(foreColor), backColor, sizeof(backColor),
			NULL, 0, NULL, 0);
	if(!GLO_NORM_FORE_COLOR)
	  GLO_NORM_FORE_COLOR = cpystr(foreColor);

	if(!GLO_NORM_BACK_COLOR)
	  GLO_NORM_BACK_COLOR = cpystr(backColor);
    }
#endif	/* _WINDOWS */
#endif	/* DOS */

    set_current_val(&vars[V_LAST_VERS_USED], TRUE, TRUE);
    /* Check for special cases first */
    if(VAR_LAST_VERS_USED
          /* Special Case #1: 3.92 use is effectively the same as 3.92 */
       && (strcmp(VAR_LAST_VERS_USED, "3.92") == 0
	   /*
	    * Special Case #2:  We're not really a new version if our
	    * version number looks like: <number><dot><number><number><alpha>
	    * The <alpha> on the end is key meaning its just a bug-fix patch.
	    */
	   || (isdigit(ALPINE_VERSION[0])
	       && ALPINE_VERSION[1] == '.'
	       && isdigit((unsigned char)ALPINE_VERSION[2])
	       && isdigit((unsigned char)ALPINE_VERSION[3])
	       && isalpha((unsigned char)ALPINE_VERSION[4])
	       && strncmp(VAR_LAST_VERS_USED, ALPINE_VERSION, 4) >= 0))){
	ps->show_new_version = 0;
    }
    /* Otherwise just do lexicographic comparision... */
    else if(VAR_LAST_VERS_USED
	    && strcmp(VAR_LAST_VERS_USED, ALPINE_VERSION) >= 0){
	ps->show_new_version = 0;
    }
    else{
        ps->pre390 = !(VAR_LAST_VERS_USED
		       && strcmp(VAR_LAST_VERS_USED, "3.90") >= 0);

#ifdef	_WINDOWS
	/*
	 * If this is the first time we've run a version > 4.40, and there
	 * is evidence that the config file has not been used by unix pine,
	 * then we convert color008 to colorlgr, color009 to colormgr, and
	 * color010 to colordgr. If the config file is being used by
	 * unix pine then color009 may really supposed to be red, etc.
	 * Same if we've already run 4.41 or higher. We don't have to do
	 * anything if we are new to alpine.
	 */
	ps->pre441 = (VAR_LAST_VERS_USED
		      && strcmp(VAR_LAST_VERS_USED, "4.40") <= 0);
#endif	/* _WINDOWS */

	/*
	 * Don't offer the new version message if we're told not to.
	 */
	set_current_val(&vars[V_NEW_VER_QUELL], TRUE, TRUE);
	ps->show_new_version = !(VAR_NEW_VER_QUELL
			         && strcmp(ALPINE_VERSION,
					   VAR_NEW_VER_QUELL) < 0);

#ifdef _WINDOWS
	if(!ps_global->install_flag)
#endif /* _WINDOWS */
	{
	if(VAR_LAST_VERS_USED){
	    strncpy(ps_global->pine_pre_vers, VAR_LAST_VERS_USED,
		    sizeof(ps_global->pine_pre_vers));
	    ps_global->pine_pre_vers[sizeof(ps_global->pine_pre_vers)-1] = '\0';
	}

	set_variable(V_LAST_VERS_USED, ALPINE_VERSION, 1, 1,
		     ps_global->ew_for_except_vars);
	}
    }

    /* Obsolete, backwards compatibility */
    set_current_val(&vars[V_ELM_STYLE_SAVE], TRUE, TRUE);
    /* Also obsolete */
    set_current_val(&vars[V_SAVE_BY_SENDER], TRUE, TRUE);
    if(!strucmp(VAR_ELM_STYLE_SAVE, "yes"))
      set_variable(V_SAVE_BY_SENDER, "yes", 1, 1, Main);
    obs_save_by_sender = !strucmp(VAR_SAVE_BY_SENDER, "yes");

    set_current_pattern_vals(ps);

    set_current_val(&vars[V_INDEX_FORMAT], TRUE, TRUE);
    init_index_format(VAR_INDEX_FORMAT, &ps->index_disp_format);

    /* this should come after pre441 is set or not */
    set_current_color_vals(ps);

    set_current_val(&vars[V_WP_INDEXHEIGHT], TRUE, TRUE);
    set_current_val(&vars[V_WP_INDEXLINES], TRUE, TRUE);
    set_current_val(&vars[V_WP_AGGSTATE], TRUE, TRUE);
    set_current_val(&vars[V_WP_STATE], TRUE, TRUE);
    set_current_val(&vars[V_WP_COLUMNS], TRUE, TRUE);

    set_current_val(&vars[V_PRUNED_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_ARCHIVED_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_INCOMING_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_SORT_KEY], TRUE, TRUE);
    if(decode_sort(VAR_SORT_KEY, &ps->def_sort, &def_sort_rev) == -1){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Sort type \"%.200s\" is invalid", VAR_SORT_KEY);
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	ps->def_sort = SortArrival;
	ps->def_sort_rev = 0;
    }
    else
      ps->def_sort_rev = def_sort_rev;

    cur_rule_value(&vars[V_SAVED_MSG_NAME_RULE], TRUE, TRUE);
    {NAMEVAL_S *v; int i;
    for(i = 0; v = save_msg_rules(i); i++)
      if(v->value == ps_global->save_msg_rule)
	break;
     
     /* if save_msg_rule is not default, or is explicitly set to default */
     if((ps_global->save_msg_rule != SAV_RULE_DEFLT) ||
	(v && v->name &&
	 (!strucmp(ps_global->vars[V_SAVED_MSG_NAME_RULE].post_user_val.p,
		   v->name) ||
	  !strucmp(ps_global->vars[V_SAVED_MSG_NAME_RULE].main_user_val.p,
		   v->name))))
       obs_save_by_sender = 0;  /* don't overwrite */
    }

    cur_rule_value(&vars[V_FCC_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_AB_SORT_RULE], TRUE, TRUE);

#ifndef	_WINDOWS
    cur_rule_value(&vars[V_COLOR_STYLE], TRUE, TRUE);
#endif

    cur_rule_value(&vars[V_INDEX_COLOR_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_TITLEBAR_COLOR_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_FLD_SORT_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_INCOMING_STARTUP], TRUE, TRUE);
    cur_rule_value(&vars[V_PRUNING_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_REOPEN_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_GOTO_DEFAULT_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_THREAD_DISP_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_THREAD_INDEX_STYLE], TRUE, TRUE);

    set_current_val(&vars[V_THREAD_MORE_CHAR], TRUE, TRUE);
    if(VAR_THREAD_MORE_CHAR[0] && VAR_THREAD_MORE_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	  _("Only using first character of threading-indicator-character option"));
	VAR_THREAD_MORE_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_THREAD_EXP_CHAR], TRUE, TRUE);
    if(VAR_THREAD_EXP_CHAR[0] && VAR_THREAD_EXP_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	   _("Only using first character of threading-expanded-character option"));
	VAR_THREAD_EXP_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_THREAD_LASTREPLY_CHAR], TRUE, TRUE);
    if(!VAR_THREAD_LASTREPLY_CHAR[0])
      VAR_THREAD_LASTREPLY_CHAR = cpystr(DF_THREAD_LASTREPLY_CHAR);

    if(VAR_THREAD_LASTREPLY_CHAR[0] && VAR_THREAD_LASTREPLY_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	  _("Only using first character of threading-lastreply-character option"));
	VAR_THREAD_LASTREPLY_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_MAXREMSTREAM], TRUE, TRUE);
    ps->s_pool.max_remstream = i = atoi(DF_MAXREMSTREAM);
    if(SVAR_MAXREMSTREAM(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->s_pool.max_remstream = i;
    
    set_current_val(&vars[V_PERMLOCKED], TRUE, TRUE);

    set_current_val(&vars[V_NMW_WIDTH], TRUE, TRUE);
    ps->nmw_width = i = atoi(DF_NMW_WIDTH);
    if(SVAR_NMW_WIDTH(ps, i, tmp_20k_buf, SIZEOF_20KBUF))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->nmw_width = i;

    /* backwards compatibility */
    if(obs_save_by_sender){
        ps->save_msg_rule = SAV_RULE_FROM;
	set_variable(V_SAVED_MSG_NAME_RULE, "by-from", 1, 1, Main);
    }

    /* this should come after process_feature_list because of use_fkeys */
    if(!ps->start_in_index)
        set_current_val(&vars[V_INIT_CMD_LIST], FALSE, TRUE);
    if(VAR_INIT_CMD_LIST && VAR_INIT_CMD_LIST[0] && VAR_INIT_CMD_LIST[0][0])
      if(cmds_f)
        (*cmds_f)(ps, VAR_INIT_CMD_LIST);

#ifdef	_WINDOWS
    mswin_set_quit_confirm (F_OFF(F_QUIT_WO_CONFIRM, ps_global));
    if(ps_global->update_registry != UREG_NEVER_SET){
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		   ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_DIR, ps_global->pine_dir, (size_t)NULL);
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		     ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_EXE, ps_global->pine_name, (size_t)NULL);
    }
#endif	/* _WINDOWS */

#ifdef DEBUG
    dump_configuration(0);
#endif /* DEBUG */
}


void
convert_configvars_to_utf8(struct variable *vars, char *fromcharset)
{
    struct variable *v;

    /*
     * Make sure that everything is UTF-8.
     */
    for(v = vars; v->name; v++)
      convert_configvar_to_utf8(v, fromcharset);
}


void
convert_configvar_to_utf8(struct variable *v, char *fromcharset)
{
    char **p, *conv, **valptr;
    int i;

    /*
     * Make sure that everything is UTF-8.
     */
    if(v->is_list){
	for(i = 0; i < 7; i++){
	    switch(i){
	      case 1: valptr = v->current_val.l; break;
	      case 0: valptr = v->main_user_val.l; break;
	      case 2: valptr = v->changed_val.l; break;
	      case 3: valptr = v->post_user_val.l; break;
	      case 4: valptr = v->global_val.l; break;
	      case 5: valptr = v->fixed_val.l; break;
	      case 6: valptr = v->cmdline_val.l; break;
	      default: panic("bad case in convert_configvar");
	    }

	    if(valptr){
		for(p = valptr; *p; p++){
		    if(**p){
			conv = convert_to_utf8(*p, fromcharset, 0);
			if(conv){
			    fs_give((void **) p);
			    *p = conv;
			}
		    }
		}
	    }
	}
    }
    else{
	for(i = 0; i < 7; i++){
	    switch(i){
	      case 1: valptr = &v->current_val.p; break;
	      case 0: valptr = &v->main_user_val.p; break;
	      case 2: valptr = &v->changed_val.p; break;
	      case 3: valptr = &v->post_user_val.p; break;
	      case 4: valptr = &v->global_val.p; break;
	      case 5: valptr = &v->fixed_val.p; break;
	      case 6: valptr = &v->cmdline_val.p; break;
	      default: panic("bad case in convert_configvar");
	    }

	    if(valptr && *valptr && (*valptr)[0]){
		conv = convert_to_utf8(*valptr, fromcharset, 0);
		if(conv){
		    fs_give((void **) valptr);
		    *valptr = conv;
		}
	    }
	}
    }
}



/*
 * Standard feature name sections
 */
char *
feature_list_section(FEATURE_S *feature)
{
#define	PREF_NONE      -1
    static char *feat_sect[] = {
#define PREF_MISC	0
	/* TRANSLATORS: section heading in configuration screen */
	N_("Advanced User Preferences"),
#define PREF_FLDR	1
	/* TRANSLATORS: section heading in configuration screen */
	N_("Folder Preferences"),
#define PREF_ADDR	2
	/* TRANSLATORS: section heading in configuration screen */
	N_("Address Book Preferences"),
#define PREF_COMP	3
	/* TRANSLATORS: section heading in configuration screen */
	N_("Composer Preferences"),
#define PREF_NEWS	4
	/* TRANSLATORS: section heading in configuration screen */
	N_("News Preferences"),
#define PREF_VIEW	5
	/* TRANSLATORS: section heading in configuration screen */
	N_("Viewer Preferences"),
#define PREF_ACMD	6
	/* TRANSLATORS: section heading in configuration screen */
	N_("Advanced Command Preferences"),
#define PREF_PRNT	7
	/* TRANSLATORS: section heading in configuration screen */
	N_("Printer Preferences"),
#define	PREF_RPLY	8
	/* TRANSLATORS: section heading in configuration screen */
	N_("Reply Preferences"),
#define	PREF_SEND	9
	/* TRANSLATORS: section heading in configuration screen */
	N_("Sending Preferences"),
#define	PREF_INDX	10
	/* TRANSLATORS: section heading in configuration screen */
	N_("Message Index Preferences"),
#define	PREF_HIDDEN	11
	HIDDEN_PREF
};

    return((feature && feature->section > PREF_NONE
	    && feature->section < (sizeof(feat_sect)/sizeof(feat_sect[0])))
	   ? _(feat_sect[feature->section]) : NULL);
}


/* any os-specific exclusions */
#if	defined(DOS) || defined(OS2)
#define	PREF_OS_LWSD	PREF_NONE
#define	PREF_OS_LCLK	PREF_NONE
#define	PREF_OS_STSP	PREF_NONE
#define	PREF_OS_SPWN	PREF_NONE
#define	PREF_OS_XNML	PREF_NONE
#define	PREF_OS_USFK	PREF_MISC
#define	PREF_OS_MOUSE	PREF_NONE
#else
#define	PREF_OS_LWSD	PREF_MISC
#define	PREF_OS_LCLK	PREF_COMP
#define	PREF_OS_STSP	PREF_MISC
#define	PREF_OS_SPWN	PREF_MISC
#define	PREF_OS_XNML	PREF_MISC
#define	PREF_OS_USFK	PREF_NONE
#define	PREF_OS_MOUSE	PREF_MISC
#endif


/*
 * Standard way to get at feature list members...
 */
FEATURE_S *
feature_list(int index)
{
    /*
     * This list is alphabatized by feature string, but the 
     * macro values need not be ordered.
     */
    static FEATURE_S feat_list[] = {
/* Composer prefs */
	{"Alternate-Compose-Menu",
	 F_ALT_COMPOSE_MENU, h_config_alt_compose_menu, PREF_COMP},
	{"Alternate-Role-Menu",
	 F_ALT_ROLE_MENU, h_config_alt_role_menu, PREF_COMP},
	{"Compose-Cancel-Confirm-Uses-Yes",
	 F_CANCEL_CONFIRM, h_config_cancel_confirm, PREF_COMP},
	{"Compose-Cut-From-Cursor",
	 F_DEL_FROM_DOT, h_config_del_from_dot, PREF_COMP},
	{"Compose-Maps-Delete-Key-To-Ctrl-D",
	 F_COMPOSE_MAPS_DEL, h_config_compose_maps_del, PREF_COMP},
	{"Compose-Rejects-Unqualified-Addrs",
	 F_COMPOSE_REJECTS_UNQUAL, h_config_compose_rejects_unqual, PREF_COMP},
	{"Compose-Send-Offers-First-Filter",
	 F_FIRST_SEND_FILTER_DFLT, h_config_send_filter_dflt, PREF_COMP},
	{"Downgrade-Multipart-To-Text",
	 F_COMPOSE_ALWAYS_DOWNGRADE, h_downgrade_multipart_to_text, PREF_COMP},
	{"Enable-Alternate-Editor-Cmd",
	 F_ENABLE_ALT_ED, h_config_enable_alt_ed, PREF_COMP},
	{"Enable-Alternate-Editor-Implicitly",
	 F_ALT_ED_NOW, h_config_alt_ed_now, PREF_COMP},
	{"Enable-Search-And-Replace",
	 F_ENABLE_SEARCH_AND_REPL, h_config_enable_search_and_repl, PREF_COMP},
	{"Enable-Sigdashes",
	 F_ENABLE_SIGDASHES, h_config_sigdashes, PREF_COMP},
	{"Quell-Dead-Letter-On-Cancel",
	 F_QUELL_DEAD_LETTER, h_config_quell_dead_letter, PREF_COMP},
	{"Quell-Flowed-Text",
	 F_QUELL_FLOWED_TEXT, h_config_quell_flowed_text, PREF_COMP},
	{"Quell-Mailchecks-Composing-Except-Inbox",
	 F_QUELL_PINGS_COMPOSING, h_config_quell_checks_comp, PREF_COMP},
	{"Quell-Mailchecks-Composing-Inbox",
	 F_QUELL_PINGS_COMPOSING_INBOX, h_config_quell_checks_comp_inbox, PREF_COMP},
	{"Quell-User-Lookup-In-Passwd-File",
	 F_QUELL_LOCAL_LOOKUP, h_config_quell_local_lookup, PREF_OS_LCLK},
	{"Spell-Check-Before-Sending",
	 F_ALWAYS_SPELL_CHECK, h_config_always_spell_check, PREF_COMP},
	{"Strip-Whitespace-Before-Send",
	 F_STRIP_WS_BEFORE_SEND, h_config_strip_ws_before_send, PREF_COMP},

/* Reply Prefs */
	{"Enable-Reply-Indent-String-Editing",
	 F_ENABLE_EDIT_REPLY_INDENT, h_config_prefix_editing, PREF_RPLY},
	{"Include-Attachments-In-Reply",
	 F_ATTACHMENTS_IN_REPLY, h_config_attach_in_reply, PREF_RPLY},
	{"Include-Header-In-Reply",
	 F_INCLUDE_HEADER, h_config_include_header, PREF_RPLY},
	{"Include-Text-In-Reply",
	 F_AUTO_INCLUDE_IN_REPLY, h_config_auto_include_reply, PREF_RPLY},
	{"Reply-Always-Uses-Reply-To",
	 F_AUTO_REPLY_TO, h_config_auto_reply_to, PREF_RPLY},
	{"Signature-At-Bottom",
	 F_SIG_AT_BOTTOM, h_config_sig_at_bottom, PREF_RPLY},
	{"Strip-From-Sigdashes-On-Reply",
	 F_ENABLE_STRIP_SIGDASHES, h_config_strip_sigdashes, PREF_RPLY},

/* Sending Prefs */
	{"Disable-Sender",
	 F_DISABLE_SENDER, h_config_disable_sender, PREF_SEND},
	{"Enable-8bit-ESMTP-Negotiation",
	 F_ENABLE_8BIT, h_config_8bit_smtp, PREF_SEND},
#ifdef	BACKGROUND_POST
	{"Enable-Background-Sending",
	 F_BACKGROUND_POST, h_config_compose_bg_post, PREF_SEND},
#endif
	{"Enable-Delivery-Status-Notification",
	 F_DSN, h_config_compose_dsn, PREF_SEND},
	{"Enable-Verbose-SMTP-Posting",
	 F_VERBOSE_POST, h_config_verbose_post, PREF_SEND},
	{"Fcc-On-Bounce",
	 F_FCC_ON_BOUNCE, h_config_fcc_on_bounce, PREF_SEND},
	{"Fcc-Only-Without-Confirm",
	 F_AUTO_FCC_ONLY, h_config_auto_fcc_only, PREF_SEND},
	{"Fcc-Without-Attachments",
	 F_NO_FCC_ATTACH, h_config_no_fcc_attach, PREF_SEND},
	{"Mark-Fcc-Seen",
	 F_MARK_FCC_SEEN, h_config_mark_fcc_seen, PREF_SEND},
	{"Send-Without-Confirm",
	 F_SEND_WO_CONFIRM, h_config_send_wo_confirm, PREF_SEND},
	{"Use-Sender-Not-X-Sender",
	 F_USE_SENDER_NOT_X, h_config_use_sender_not_x, PREF_SEND},
	{"Warn-If-Blank-To-and-Cc-and-Newsgroups",
	 F_WARN_ABOUT_NO_TO_OR_CC, h_config_warn_if_no_to_or_cc, PREF_SEND},
	{"Warn-If-Blank-Subject",
	 F_WARN_ABOUT_NO_SUBJECT, h_config_warn_if_subj_blank, PREF_SEND},

/* Folder */
	{"Combined-Subdirectory-Display",
	 F_CMBND_SUBDIR_DISP, h_config_combined_subdir_display, PREF_FLDR},
	{"Combined-Folder-Display",
	 F_CMBND_FOLDER_DISP, h_config_combined_folder_display, PREF_FLDR},
	{"Enable-Dot-Folders",
	 F_ENABLE_DOT_FOLDERS, h_config_enable_dot_folders, PREF_FLDR},
	{"Enable-Incoming-Folders",
	 F_ENABLE_INCOMING, h_config_enable_incoming, PREF_FLDR},
	{"Enable-Incoming-Folders-Checking",
	 F_ENABLE_INCOMING_UNSEEN, h_config_enable_incoming_checking, PREF_FLDR},
	{"Enable-Lame-List-Mode",
	 F_FIX_BROKEN_LIST, h_config_lame_list_mode, PREF_FLDR},
	{"Expanded-View-Of-Folders",
	 F_EXPANDED_FOLDERS, h_config_expanded_folders, PREF_FLDR},
	{"Quell-Empty-Directories",
	 F_QUELL_EMPTY_DIRS, h_config_quell_empty_dirs, PREF_FLDR},
	{"Separate-Folder-and-Directory-Entries",
	 F_SEPARATE_FLDR_AS_DIR, h_config_separate_fold_dir_view, PREF_FLDR},
	{"Single-Column-Folder-List",
	 F_SINGLE_FOLDER_LIST, h_config_single_list, PREF_FLDR},
	{"Sort-Default-Fcc-Alpha",
	 F_SORT_DEFAULT_FCC_ALPHA, h_config_sort_fcc_alpha, PREF_FLDR},
	{"Sort-Default-Save-Alpha",
	 F_SORT_DEFAULT_SAVE_ALPHA, h_config_sort_save_alpha, PREF_FLDR},
	{"Vertical-Folder-List",
	 F_VERTICAL_FOLDER_LIST, h_config_vertical_list, PREF_FLDR},

/* Addr book */
	{"Combined-Addrbook-Display",
	 F_CMBND_ABOOK_DISP, h_config_combined_abook_display, PREF_ADDR},
	{"Expanded-View-of-Addressbooks",
	 F_EXPANDED_ADDRBOOKS, h_config_expanded_addrbooks, PREF_ADDR},
	{"Expanded-View-of-Distribution-Lists",
	 F_EXPANDED_DISTLISTS, h_config_expanded_distlists, PREF_ADDR},
#ifdef	ENABLE_LDAP
	{"LDAP-Result-to-Addrbook-Add",
	 F_ADD_LDAP_TO_ABOOK, h_config_add_ldap, PREF_ADDR},
#endif

/* Index prefs */
	{"Auto-Open-Next-Unread",
	 F_AUTO_OPEN_NEXT_UNREAD, h_config_auto_open_unread, PREF_INDX},
	{"Continue-Tab-Without-Confirm",
	 F_TAB_NO_CONFIRM, h_config_tab_no_prompt, PREF_INDX},
	{"Delete-Skips-Deleted",
	 F_DEL_SKIPS_DEL, h_config_del_skips_del, PREF_INDX},
	{"Disable-Index-Locale-Dates",
	 F_DISABLE_INDEX_LOCALE_DATES, h_config_disable_index_locale_dates, PREF_INDX},
	{"Enable-Cruise-Mode",
	 F_ENABLE_SPACE_AS_TAB, h_config_cruise_mode, PREF_INDX},
	{"Enable-Cruise-Mode-Delete",
	 F_ENABLE_TAB_DELETES, h_config_cruise_mode_delete, PREF_INDX},
	{"Mark-For-Cc",
	 F_MARK_FOR_CC, h_config_mark_for_cc, PREF_INDX},
	{"Next-Thread-Without-Confirm",
	 F_NEXT_THRD_WO_CONFIRM, h_config_next_thrd_wo_confirm, PREF_INDX},
	{"Return-to-Inbox-Without-Confirm",
	 F_RET_INBOX_NO_CONFIRM, h_config_inbox_no_confirm, PREF_INDX},
	{"Show-Sort",
	 F_SHOW_SORT, h_config_show_sort, PREF_INDX},
	{"Tab-Uses-Unseen-For-Next-Folder",
	 F_TAB_USES_UNSEEN, h_config_tab_uses_unseen, PREF_INDX},
	{"Tab-Visits-Next-New-Message-Only",
	 F_TAB_TO_NEW, h_config_tab_new_only, PREF_INDX},
	{"Thread-Index-Shows-Important-Color",
	 F_COLOR_LINE_IMPORTANT, h_config_color_thrd_import, PREF_INDX},

/* Viewer prefs */
	{"Enable-Msg-View-Addresses",
	 F_SCAN_ADDR, h_config_enable_view_addresses, PREF_VIEW},
	{"Enable-Msg-View-Attachments",
	 F_VIEW_SEL_ATTACH, h_config_enable_view_attach, PREF_VIEW},
	{"Enable-Msg-View-Forced-Arrows",
	 F_FORCE_ARROWS, h_config_enable_view_arrows, PREF_VIEW},
	{"Enable-Msg-View-URLs",
	 F_VIEW_SEL_URL, h_config_enable_view_url, PREF_VIEW},
	{"Enable-Msg-View-Web-Hostnames",
	 F_VIEW_SEL_URL_HOST, h_config_enable_view_web_host, PREF_VIEW},
	{"Prefer-Plain-Text",
	 F_PREFER_PLAIN_TEXT, h_config_prefer_plain_text, PREF_VIEW},
	/* set to TRUE for windows */
	{"Pass-C1-Control-Characters-as-is",
	 F_PASS_C1_CONTROL_CHARS, h_config_pass_c1_control, PREF_VIEW},
	{"Pass-Control-Characters-as-is",
	 F_PASS_CONTROL_CHARS, h_config_pass_control, PREF_VIEW},
	{"Quell-Charset-Warning",
	 F_QUELL_CHARSET_WARNING, h_config_quell_charset_warning, PREF_VIEW},
	{"Quell-Server-After-Link-in-HTML",
	 F_QUELL_HOST_AFTER_URL, h_config_quell_host_after_url, PREF_VIEW},

/* News */
	{"Compose-Sets-Newsgroup-Without-Confirm",
	 F_COMPOSE_TO_NEWSGRP, h_config_compose_news_wo_conf, PREF_NEWS},
	{"Enable-8bit-NNTP-Posting",
	 F_ENABLE_8BIT_NNTP, h_config_8bit_nntp, PREF_NEWS},
	{"Enable-Multiple-Newsrcs",
	 F_ENABLE_MULNEWSRCS, h_config_enable_mulnewsrcs, PREF_NEWS},
	{"Hide-NNTP-Path",
	 F_HIDE_NNTP_PATH, h_config_hide_nntp_path, PREF_NEWS},
	{"Mult-Newsrc-Hostnames-as-Typed",
	 F_MULNEWSRC_HOSTNAMES_AS_TYPED, h_config_mulnews_as_typed, PREF_NEWS},
	{"News-Approximates-New-Status",
	 F_FAKE_NEW_IN_NEWS, h_config_news_uses_recent, PREF_NEWS},
	{"News-Deletes-Across-Groups",
	 F_NEWS_CROSS_DELETE, h_config_news_cross_deletes, PREF_NEWS},
	{"News-Offers-Catchup-on-Close",
	 F_NEWS_CATCHUP, h_config_news_catchup, PREF_NEWS},
	{"News-Post-Without-Validation",
	 F_NO_NEWS_VALIDATION, h_config_post_wo_validation, PREF_NEWS},
	{"News-Read-in-Newsrc-Order",
	 F_READ_IN_NEWSRC_ORDER, h_config_read_in_newsrc_order, PREF_NEWS},
	{"Predict-NNTP-Server",
	 F_PREDICT_NNTP_SERVER, h_config_predict_nntp_server, PREF_NEWS},
	{"Quell-Extra-Post-Prompt",
	 F_QUELL_EXTRA_POST_PROMPT, h_config_quell_post_prompt, PREF_NEWS},

/* Print */
	{"Enable-Print-Via-Y-Command",
	 F_ENABLE_PRYNT, h_config_enable_y_print, PREF_PRNT},
	{"Print-Formfeed-Between-Messages",
	 F_AGG_PRINT_FF, h_config_ff_between_msgs, PREF_PRNT},
	{"Print-Includes-From-Line",
	 F_FROM_DELIM_IN_PRINT, h_config_print_from, PREF_PRNT},
	{"Print-Index-Enabled",
	 F_PRINT_INDEX, h_config_print_index, PREF_PRNT},
	{"Print-Offers-Custom-Cmd-Prompt",
	 F_CUSTOM_PRINT, h_config_custom_print, PREF_PRNT},

/* adv cmd prefs */
	{"Enable-Aggregate-Command-Set",
	 F_ENABLE_AGG_OPS, h_config_enable_agg_ops, PREF_ACMD},
	{"Enable-Arrow-Navigation",
	 F_ARROW_NAV, h_config_arrow_nav, PREF_ACMD},
	{"Enable-Arrow-Navigation-Relaxed",
	 F_RELAXED_ARROW_NAV, h_config_relaxed_arrow_nav, PREF_ACMD},
	{"Enable-Bounce-Cmd",
	 F_ENABLE_BOUNCE, h_config_enable_bounce, PREF_ACMD},
	{"Enable-Exit-Via-Lessthan-Command",
	 F_ENABLE_LESSTHAN_EXIT, h_config_enable_lessthan_exit, PREF_ACMD},
	{"Enable-Flag-Cmd",
	 F_ENABLE_FLAG, h_config_enable_flag, PREF_ACMD},
	{"Enable-Flag-Screen-Implicitly",
	 F_FLAG_SCREEN_DFLT, h_config_flag_screen_default, PREF_ACMD},
	{"Enable-Flag-Screen-Keyword-Shortcut",
	 F_FLAG_SCREEN_KW_SHORTCUT, h_config_flag_screen_kw_shortcut,PREF_ACMD},
	{"Enable-Full-Header-and-Text",
	 F_ENABLE_FULL_HDR_AND_TEXT, h_config_enable_full_hdr_and_text, PREF_ACMD},
	{"Enable-Full-Header-Cmd",
	 F_ENABLE_FULL_HDR, h_config_enable_full_hdr, PREF_ACMD},
	{"Enable-Goto-in-File-Browser",
	 F_ALLOW_GOTO, h_config_allow_goto, PREF_ACMD},
	{"Enable-Jump-Shortcut",
	 F_ENABLE_JUMP, h_config_enable_jump, PREF_ACMD},
	{"Enable-Partial-Match-Lists",
	 F_ENABLE_SUB_LISTS, h_config_sub_lists, PREF_ACMD},
	{"Enable-Tab-Completion",
	 F_ENABLE_TAB_COMPLETE, h_config_enable_tab_complete, PREF_ACMD},
	{"Enable-Unix-Pipe-Cmd",
	 F_ENABLE_PIPE, h_config_enable_pipe, PREF_ACMD},
	{"Quell-Full-Header-Auto-Reset",
	 F_QUELL_FULL_HDR_RESET, h_config_quell_full_hdr_reset, PREF_ACMD},

/* Adv user prefs */
#if !defined(DOS) && !defined(OS2)
	{"Allow-Talk",
	 F_ALLOW_TALK, h_config_allow_talk, PREF_MISC},
#endif
	{"Assume-Slow-Link",
	 F_FORCE_LOW_SPEED, h_config_force_low_speed, PREF_OS_LWSD},
	{"Auto-Move-Read-Msgs",
	 F_AUTO_READ_MSGS, h_config_auto_read_msgs, PREF_MISC},
	{"Auto-Unselect-After-Apply",
	 F_AUTO_UNSELECT, h_config_auto_unselect, PREF_MISC},
	{"Auto-Unzoom-After-Apply",
	 F_AUTO_UNZOOM, h_config_auto_unzoom, PREF_MISC},
	{"Auto-Zoom-After-Select",
	 F_AUTO_ZOOM, h_config_auto_zoom, PREF_MISC},
	{"Busy-Cue-Spinner-Only",
	 F_USE_BORING_SPINNER, h_config_use_boring_spinner, PREF_MISC},
	{"Check-Newmail-When-Quitting",
	 F_CHECK_MAIL_ONQUIT, h_config_check_mail_onquit, PREF_MISC},
	{"Confirm-Role-Even-For-Default",
	 F_ROLE_CONFIRM_DEFAULT, h_config_confirm_role, PREF_MISC},
	{"Disable-Input-History",
	 F_DISABLE_INPUT_HISTORY, h_config_input_history, PREF_MISC},
	{"Disable-Keymenu",
	 F_BLANK_KEYMENU, h_config_blank_keymenu, PREF_MISC},
	{"Disable-Take-Fullname-in-Addresses",
	 F_DISABLE_TAKE_FULLNAMES, h_config_take_fullname, PREF_MISC},
	{"Disable-Take-Last-Comma-First",
	 F_DISABLE_TAKE_LASTFIRST, h_config_take_lastfirst, PREF_MISC},
	{"Disable-Terminal-Reset-For-Display-Filters",
	 F_DISABLE_TERM_RESET_DISP, h_config_disable_reset_disp, PREF_MISC},
	{"Enable-Dot-Files",
	 F_ENABLE_DOT_FILES, h_config_enable_dot_files, PREF_MISC},
	{"Enable-Fast-Recent-Test",
	 F_ENABLE_FAST_RECENT, h_config_fast_recent, PREF_MISC},
	{"Enable-Mail-Check-Cue",
	 F_SHOW_DELAY_CUE, h_config_show_delay_cue, PREF_MISC},
	{"Enable-Mouse-in-Xterm",
	 F_ENABLE_MOUSE, h_config_enable_mouse, PREF_OS_MOUSE},
	{"Enable-Newmail-in-Xterm-Icon",
	 F_ENABLE_XTERM_NEWMAIL, h_config_enable_xterm_newmail, PREF_OS_XNML},
	{"Enable-Newmail-Short-Text-in-Icon",
	 F_ENABLE_NEWMAIL_SHORT_TEXT, h_config_enable_newmail_short_text, PREF_OS_XNML},
	{"Enable-Rules-Under-Take",
	 F_ENABLE_ROLE_TAKE, h_config_enable_role_take, PREF_MISC},
	{"Enable-Suspend",
	 F_CAN_SUSPEND, h_config_can_suspend, PREF_MISC},
	{"Enable-Take-Export",
	 F_ENABLE_TAKE_EXPORT, h_config_enable_take_export, PREF_MISC},
#ifdef	_WINDOWS
	{"Enable-Tray-Icon",
	 F_ENABLE_TRAYICON, h_config_tray_icon, PREF_MISC},
#endif
	{"Expose-Hidden-Config",
	 F_EXPOSE_HIDDEN_CONFIG, h_config_expose_hidden_config, PREF_MISC},
	{"Expunge-Only-Manually",
	 F_EXPUNGE_MANUALLY, h_config_expunge_manually, PREF_MISC},
	{"Expunge-Without-Confirm",
	 F_AUTO_EXPUNGE, h_config_auto_expunge, PREF_MISC},
	{"Expunge-Without-Confirm-Everywhere",
	 F_FULL_AUTO_EXPUNGE, h_config_full_auto_expunge, PREF_MISC},
	{"Force-Arrow-Cursor",
	 F_FORCE_ARROW, h_config_force_arrow, PREF_MISC},
	{"Maildrops-Preserve-State",
	 F_MAILDROPS_PRESERVE_STATE, h_config_maildrops_preserve_state,
	 PREF_MISC},
	{"Offer-Expunge-of-Inbox",
	 F_EXPUNGE_INBOX, h_config_expunge_inbox, PREF_MISC},
	{"Offer-Expunge-of-Stayopen-Folders",
	 F_EXPUNGE_STAYOPENS, h_config_expunge_stayopens, PREF_MISC},
	{"Preopen-Stayopen-Folders",
	 F_PREOPEN_STAYOPENS, h_config_preopen_stayopens, PREF_MISC},
	{"Preserve-Start-Stop-Characters",
	 F_PRESERVE_START_STOP, h_config_preserve_start_stop, PREF_OS_STSP},
	{"Prune-Uses-YYYY-MM",
	 F_PRUNE_USES_ISO, h_config_prune_uses_iso, PREF_MISC},
	{"Quell-Attachment-Extension-Warn",
	 F_QUELL_ATTACH_EXT_WARN, h_config_quell_attach_ext_warn,
	 PREF_MISC},
	{"Quell-Attachment-Extra-Prompt",
	 F_QUELL_ATTACH_EXTRA_PROMPT, h_config_quell_attach_extra_prompt,
	 PREF_MISC},
	{"Quell-Content-ID",
	 F_QUELL_CONTENT_ID, h_config_quell_content_id, PREF_MISC},
	{"Quell-Filtering-Done-Message",
	 F_QUELL_FILTER_DONE_MSG, h_config_quell_filtering_done_message,
	 PREF_MISC},
	{"Quell-Filtering-Messages",
	 F_QUELL_FILTER_MSGS, h_config_quell_filtering_messages,
	 PREF_MISC},
	{"Quell-Timezone-Comment-When-Sending",
	 F_QUELL_TIMEZONE, h_config_quell_tz_comment, PREF_MISC},
	{"Quell-Folder-Internal-Msg",
	 F_QUELL_INTERNAL_MSG, h_config_quell_folder_internal_msg, PREF_MISC},
	{"Quell-Lock-Failure-Warnings",
	 F_QUELL_LOCK_FAILURE_MSGS, h_config_quell_lock_failure_warnings,
	 PREF_MISC},
#ifdef	_WINDOWS
	{"Quell-SSL-Largeblocks",
	 F_QUELL_SSL_LARGEBLOCKS, h_config_quell_ssl_largeblocks, PREF_MISC},
#endif
	{"Quell-Status-Message-Beeping",
	 F_QUELL_BEEPS, h_config_quell_beeps, PREF_MISC},
	{"Quit-Without-Confirm",
	 F_QUIT_WO_CONFIRM, h_config_quit_wo_confirm, PREF_MISC},
	{"Quote-Replace-Nonflowed",
	 F_QUOTE_REPLACE_NOFLOW, h_config_quote_replace_noflow, PREF_MISC},
	{"Save-Partial-Msg-Without-Confirm",
	 F_SAVE_PARTIAL_WO_CONFIRM, h_config_save_part_wo_confirm, PREF_MISC},
	{"Save-Will-Advance",
	 F_SAVE_ADVANCES, h_config_save_advances, PREF_MISC},
	{"Save-Will-Not-Delete",
	 F_SAVE_WONT_DELETE, h_config_save_wont_delete, PREF_MISC},
	{"Save-Will-Quote-Leading-Froms",
	 F_QUOTE_ALL_FROMS, h_config_quote_all_froms, PREF_MISC},
	{"Scramble-Message-ID",
	 F_ROT13_MESSAGE_ID, h_config_scramble_message_id, PREF_MISC},
	{"Select-Without-Confirm",
	 F_SELECT_WO_CONFIRM, h_config_select_wo_confirm, PREF_MISC},
	{"Show-Cursor",
	 F_SHOW_CURSOR, h_config_show_cursor, PREF_MISC},
	{"Show-Plain-Text-Internally",
	 F_SHOW_TEXTPLAIN_INT, h_config_textplain_int, PREF_MISC},
	{"Show-Selected-in-Boldface",
	 F_SELECTED_SHOWN_BOLD, h_config_select_in_bold, PREF_MISC},
	{"Slash-Collapses-Entire-Thread",
	 F_SLASH_COLL_ENTIRE, h_config_slash_coll_entire, PREF_MISC},
#ifdef	_WINDOWS
	{"Store-Window-Position-in-Config",
	 F_STORE_WINPOS_IN_CONFIG, h_config_winpos_in_config, PREF_MISC},
#endif
	{"Tab-Checks-Recent",
	 F_TAB_CHK_RECENT, h_config_tab_checks_recent, PREF_MISC},
	{"Try-Alternative-Authentication-Driver-First",
	 F_PREFER_ALT_AUTH, h_config_alt_auth, PREF_MISC},
	{"Unselect-Will-Not-Advance",
	 F_UNSELECT_WONT_ADVANCE, h_config_unsel_wont_advance, PREF_MISC},
	{"Use-Current-Dir",
	 F_USE_CURRENT_DIR, h_config_use_current_dir, PREF_MISC},
	{"Use-Function-Keys",
	 F_USE_FK, h_config_use_fk, PREF_OS_USFK},
	{"Use-Regular-Startup-Rule-For-Stayopen-Folders",
	 F_STARTUP_STAYOPEN, h_config_use_reg_start_for_stayopen, PREF_MISC},
	{"Use-Subshell-For-Suspend",
	 F_SUSPEND_SPAWNS, h_config_suspend_spawns, PREF_OS_SPWN},
#ifndef	_WINDOWS
	{"Use-System-Translation",
	 F_USE_SYSTEM_TRANS, h_config_use_system_translation, PREF_MISC},
#endif

/* Hidden Features */
	{"Old-Growth",
	 F_OLD_GROWTH, NO_HELP, PREF_NONE},
	{"Allow-Changing-From",
	 F_ALLOW_CHANGING_FROM, h_config_allow_chg_from, PREF_HIDDEN},
	{"Disable-Config-Cmd",
	 F_DISABLE_CONFIG_SCREEN, h_config_disable_config_cmd, PREF_HIDDEN},
	{"Disable-Keyboard-Lock-Cmd",
	 F_DISABLE_KBLOCK_CMD, h_config_disable_kb_lock, PREF_HIDDEN},
	{"Disable-Password-Caching",
	 F_DISABLE_PASSWORD_CACHING, h_config_disable_password_caching,
	 PREF_HIDDEN},
	{"Disable-Password-Cmd",
	 F_DISABLE_PASSWORD_CMD, h_config_disable_password_cmd, PREF_HIDDEN},
	{"Disable-Pipes-in-Sigs",
	 F_DISABLE_PIPES_IN_SIGS, h_config_disable_pipes_in_sigs, PREF_HIDDEN},
	{"Disable-Pipes-in-Templates",
	 F_DISABLE_PIPES_IN_TEMPLATES, h_config_disable_pipes_in_templates,
	 PREF_HIDDEN},
	{"Disable-Roles-Setup-Cmd",
	 F_DISABLE_ROLES_SETUP, h_config_disable_roles_setup, PREF_HIDDEN},
	{"Disable-Roles-Sig-Edit",
	 F_DISABLE_ROLES_SIGEDIT, h_config_disable_roles_sigedit, PREF_HIDDEN},
	{"Disable-Roles-Template-Edit",
	 F_DISABLE_ROLES_TEMPLEDIT, h_config_disable_roles_templateedit,
	 PREF_HIDDEN},
	{"Disable-Setlocale-Collate",
	 F_DISABLE_SETLOCALE_COLLATE, h_config_disable_collate, PREF_HIDDEN},
	{"Disable-Shared-Namespaces",
	 F_DISABLE_SHARED_NAMESPACES, h_config_disable_shared, PREF_HIDDEN},
	{"Disable-Signature-Edit-Cmd",
	 F_DISABLE_SIGEDIT_CMD, h_config_disable_signature_edit, PREF_HIDDEN},
	{"Enable-Mailcap-Param-Substitution",
	 F_DO_MAILCAP_PARAM_SUBST, h_config_mailcap_params, PREF_HIDDEN},
	{"Quell-Berkeley-Format-Timezone",
	 F_QUELL_BEZERK_TIMEZONE, h_config_no_bezerk_zone, PREF_HIDDEN},
	{"Quell-IMAP-Envelope-Update",
	 F_QUELL_IMAP_ENV_CB, h_config_quell_imap_env, PREF_HIDDEN},
	{"Quell-Maildomain-Warning",
	 F_QUELL_MAILDOMAIN_WARNING, h_config_quell_domain_warn, PREF_HIDDEN},
	{"Quell-News-Envelope-Update",
	 F_QUELL_NEWS_ENV_CB, h_config_quell_news_env, PREF_HIDDEN},
	{"Quell-Partial-Fetching",
	 F_QUELL_PARTIAL_FETCH, h_config_quell_partial, PREF_HIDDEN},
	{"Quell-Personal-Name-Prompt",
	 F_QUELL_PERSONAL_NAME_PROMPT, h_config_quell_personal_name_prompt, PREF_HIDDEN},
	{"Quell-User-ID-Prompt",
	 F_QUELL_USER_ID_PROMPT, h_config_quell_user_id_prompt, PREF_HIDDEN},
	{"Save-Aggregates-Copy-Sequence",
	 F_AGG_SEQ_COPY, h_config_save_aggregates, PREF_HIDDEN},
	{"Selectable-Item-Nobold",
	 F_SLCTBL_ITEM_NOBOLD, NO_HELP, PREF_NONE},
	{"Termdef-Takes-Precedence",
	 F_TCAP_WINS, h_config_termcap_wins, PREF_HIDDEN},
	{"Send-Confirms-Only-Expanded",
	 F_SEND_CONFIRM_ON_EXPAND, 0, PREF_HIDDEN},	/* exposed in Web Alpine */
	{"Enable-Jump-Cmd",
	 F_ENABLE_JUMP_CMD, 0, PREF_HIDDEN},		/* exposed in Web Alpine */
	{"Enable-Newmail-Sound",
	 F_ENABLE_NEWMAIL_SOUND, 0, PREF_HIDDEN}	/* exposed in Web Alpine */
    };

    return((index >= 0 && index < (sizeof(feat_list)/sizeof(feat_list[0])))
	   ? &feat_list[index] : NULL);
}


/*
 * feature_list_index -- return index of given feature id in
 *			 feature list
 */
int
feature_list_index(int id)
{
    FEATURE_S *feature;
    int	       i;

    for(i = 0; feature = feature_list(i); i++)
      if(id == feature->id)
	return(i);

    return(-1);
}


/*
 * feature_list_name -- return the given feature id's corresponding name
 */
char *
feature_list_name(int id)
{
    FEATURE_S *f;

    return((f = feature_list(feature_list_index(id))) ? f->name : "");
}


/*
 * feature_list_help -- return the given feature id's corresponding help
 */
HelpType
feature_list_help(int id)
{
    FEATURE_S *f;

    return((f = feature_list(feature_list_index(id))) ? f->help : NO_HELP);
}


/*
 * All the arguments past "list" are the backwards compatibility hacks.
 */
void
process_feature_list(struct pine *ps, char **list, int old_growth, int hir, int osr)
{
    register char            *q;
    char                    **p,
                             *lvalue[BM_SIZE * 8];
    int                       i,
                              yorn;
    long                      l;
    FEATURE_S		     *feat;


    /* clear all previous settings and then reset them */
    for(i = 0; (feat = feature_list(i)) != NULL; i++)
      F_SET(feat->id, ps, 0);


    /* backwards compatibility */
    if(hir)
	F_TURN_ON(F_INCLUDE_HEADER, ps);

    /* ditto */
    if(osr)
	F_TURN_ON(F_SIG_AT_BOTTOM, ps);

    /* ditto */
    if(old_growth)
        set_old_growth_bits(ps, 0);

    /* now run through the list (global, user, and cmd_line lists are here) */
    if(list){
      for(p = list; (q = *p) != NULL; p++){
	if(struncmp(q, "no-", 3) == 0){
	  yorn = 0;
	  q += 3;
	}else{
	  yorn = 1;
	}

	for(i = 0; (feat = feature_list(i)) != NULL; i++){
	  if(strucmp(q, feat->name) == 0){
	    if(feat->id == F_OLD_GROWTH){
	      set_old_growth_bits(ps, yorn);
	    }else{
	      F_SET(feat->id, ps, yorn);
	    }
	    break;
	  }
	}
	/* if it wasn't in that list */
	if(feat == NULL)
          dprint((1,"Unrecognized feature in feature-list (%s%s)\n",
		     (yorn ? "" : "no-"), q ? q : "?"));
      }
    }

    /*
     * Turn on gratuitous '>From ' quoting, if requested...
     */
    mail_parameters(NULL, SET_FROMWIDGET,
		    F_ON(F_QUOTE_ALL_FROMS, ps) ? VOIDT : NIL);

    /*
     * Turn off .lock creation complaints...
     */
    if(F_ON(F_QUELL_LOCK_FAILURE_MSGS, ps))
      mail_parameters(NULL, SET_LOCKEACCESERROR, (void *) 0);

    /*
     * Turn on quelling of pseudo message.
     */
    if(F_ON(F_QUELL_INTERNAL_MSG,ps_global))
      mail_parameters(NULL, SET_USERHASNOLIFE, (void *) 1);

    l = F_ON(F_MULNEWSRC_HOSTNAMES_AS_TYPED,ps_global) ? 0L : 1L;
    mail_parameters(NULL, SET_NEWSRCCANONHOST, (void *) l);

    ps->pass_ctrl_chars = F_ON(F_PASS_CONTROL_CHARS,ps_global) ? 1 : 0;
    ps->pass_c1_ctrl_chars = F_ON(F_PASS_C1_CONTROL_CHARS,ps_global) ? 1 : 0;

#ifndef	_WINDOWS
    if(F_ON(F_QUELL_BEZERK_TIMEZONE,ps_global))
      mail_parameters(NULL, SET_NOTIMEZONES, (void *) 1);
#endif

    if(F_ON(F_USE_FK, ps))
      ps->orig_use_fkeys = 1;

    /* Will we have to build a new list? */
    if(!(old_growth || hir || osr))
	return;

    /*
     * Build a new list for feature-list.  The only reason we ever need to
     * do this is if one of the obsolete options is being converted
     * into a feature-list item, and it isn't already included in the user's
     * feature-list.
     */
    i = 0;
    for(p = LVAL(&ps->vars[V_FEATURE_LIST], Main);
	p && (q = *p); p++){
      /* already have it or cancelled it, don't need to add later */
      if(hir && (strucmp(q, "include-header-in-reply") == 0 ||
                             strucmp(q, "no-include-header-in-reply") == 0)){
	hir = 0;
      }else if(osr && (strucmp(q, "signature-at-bottom") == 0 ||
                             strucmp(q, "no-signature-at-bottom") == 0)){
	osr = 0;
      }else if(old_growth && (strucmp(q, "old-growth") == 0 ||
                             strucmp(q, "no-old-growth") == 0)){
	old_growth = 0;
      }
      lvalue[i++] = cpystr(q);
    }

    /* check to see if we still need to build a new list */
    if(!(old_growth || hir || osr))
	return;

    if(hir)
      lvalue[i++] = "include-header-in-reply";
    if(osr)
      lvalue[i++] = "signature-at-bottom";
    if(old_growth)
      lvalue[i++] = "old-growth";
    lvalue[i] = NULL;
    set_variable_list(V_FEATURE_LIST, lvalue, TRUE, Main);
}


void
set_current_pattern_vals(struct pine *ps)
{
    struct variable *vars = ps->vars;

    set_current_val(&vars[V_PATTERNS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_ROLES], TRUE, TRUE);
    set_current_val(&vars[V_PAT_FILTS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_FILTS_OLD], TRUE, TRUE);
    set_current_val(&vars[V_PAT_SCORES], TRUE, TRUE);
    set_current_val(&vars[V_PAT_SCORES_OLD], TRUE, TRUE);
    set_current_val(&vars[V_PAT_INCOLS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_OTHER], TRUE, TRUE);

    /*
     * If old pattern variable (V_PATTERNS) is set and the new ones aren't
     * in the config file, then convert the old data into the new variables.
     * It isn't quite that simple, though, because we don't store unset
     * variables in remote pinercs. Check for the variables but if we
     * don't find any of them, also check the version number. This change was
     * made in version 4.30. We could just check that except that we're
     * worried somebody will make an incompatible version number change in
     * their local version, and will break this. So we check both the
     * version # and the var_in_pinerc things to be safer.
     */
    if(vars[V_PATTERNS].current_val.l
       && vars[V_PATTERNS].current_val.l[0]
       && !var_in_pinerc(vars[V_PAT_ROLES].name)
       && !var_in_pinerc(vars[V_PAT_FILTS].name)
       && !var_in_pinerc(vars[V_PAT_FILTS_OLD].name)
       && !var_in_pinerc(vars[V_PAT_SCORES].name)
       && !var_in_pinerc(vars[V_PAT_SCORES_OLD].name)
       && !var_in_pinerc(vars[V_PAT_INCOLS].name)
       && isdigit((unsigned char) ps->pine_pre_vers[0])
       && ps->pine_pre_vers[1] == '.'
       && isdigit((unsigned char) ps->pine_pre_vers[2])
       && isdigit((unsigned char) ps->pine_pre_vers[3])
       && strncmp(ps->pine_pre_vers, "4.30", 4) < 0){
	convert_pattern_data();
    }

    /*
     * Otherwise, if FILTS_OLD is set and FILTS isn't in the config file,
     * convert FILTS_OLD to FILTS. Same for SCORES.
     * The reason FILTS was changed was so we could change the
     * semantics of how rules work when there are pieces in the rule that
     * we don't understand. At the same time as the FILTS change we added
     * a rule to detect 8bitSubjects. So a user might have a filter that
     * deletes messages with 8bitSubjects. The problem is that that same
     * filter in a FILTS_OLD pine would match because it would ignore the
     * 8bitSubject part of the pattern and match on the rest. So we changed
     * the semantics so that rules with unknown bits would be ignored
     * instead of used. We had to change variable names at the same time
     * because we were adding the 8bit thing and the old pines are still
     * out there. Filters and Scores can both be dangerous. Roles, Colors,
     * and Other seem less dangerous so not worth adding a new variable.
     * This was changed in 4.50.
     */
    else{
	if(vars[V_PAT_FILTS_OLD].current_val.l
	   && vars[V_PAT_FILTS_OLD].current_val.l[0]
	   && !var_in_pinerc(vars[V_PAT_FILTS].name)
	   && !var_in_pinerc(vars[V_PAT_SCORES].name)
	   && isdigit((unsigned char) ps->pine_pre_vers[0])
	   && ps->pine_pre_vers[1] == '.'
	   && isdigit((unsigned char) ps->pine_pre_vers[2])
	   && isdigit((unsigned char) ps->pine_pre_vers[3])
	   && strncmp(ps->pine_pre_vers, "4.50", 4) < 0){
	    convert_filts_pattern_data();
	}

	if(vars[V_PAT_SCORES_OLD].current_val.l
	   && vars[V_PAT_SCORES_OLD].current_val.l[0]
	   && !var_in_pinerc(vars[V_PAT_FILTS].name)
	   && !var_in_pinerc(vars[V_PAT_SCORES].name)
	   && isdigit((unsigned char) ps->pine_pre_vers[0])
	   && ps->pine_pre_vers[1] == '.'
	   && isdigit((unsigned char) ps->pine_pre_vers[2])
	   && isdigit((unsigned char) ps->pine_pre_vers[3])
	   && strncmp(ps->pine_pre_vers, "4.50", 4) < 0){
	    convert_scores_pattern_data();
	}
    }

    if(vars[V_PAT_ROLES].post_user_val.l)
      ps_global->ew_for_role_take = Post;
    else
      ps_global->ew_for_role_take = Main;

    if(vars[V_PAT_FILTS].post_user_val.l)
      ps_global->ew_for_filter_take = Post;
    else
      ps_global->ew_for_filter_take = Main;

    if(vars[V_PAT_SCORES].post_user_val.l)
      ps_global->ew_for_score_take = Post;
    else
      ps_global->ew_for_score_take = Main;

    if(vars[V_PAT_INCOLS].post_user_val.l)
      ps_global->ew_for_incol_take = Post;
    else
      ps_global->ew_for_incol_take = Main;

    if(vars[V_PAT_OTHER].post_user_val.l)
      ps_global->ew_for_other_take = Post;
    else
      ps_global->ew_for_other_take = Main;
}


/*
 * Foreach of the config files;
 * transfer the data to the new variables.
 */
void
convert_pattern_data(void)
{
    convert_pinerc_patterns(PAT_USE_MAIN);
    convert_pinerc_patterns(PAT_USE_POST);
}


void
convert_filts_pattern_data(void)
{
    convert_pinerc_filts_patterns(PAT_USE_MAIN);
    convert_pinerc_filts_patterns(PAT_USE_POST);
}


void
convert_scores_pattern_data(void)
{
    convert_pinerc_scores_patterns(PAT_USE_MAIN);
    convert_pinerc_scores_patterns(PAT_USE_POST);
}


/*
 * Foreach of the four variables, transfer the data for this config file
 * from the old patterns variable. We don't have to convert OTHER patterns
 * because they didn't exist in pines without patterns-other.
 *
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_patterns(long int use_flags)
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_PAT | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint((2, "converting old patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_role &&
		   add_to_pattern(pat, ROLE_DO_ROLES | use_flags))
		  rflags |= ROLE_DO_ROLES;
		if(act->is_a_incol &&
		   add_to_pattern(pat, ROLE_DO_INCOLS | use_flags))
		  rflags |= ROLE_DO_INCOLS;
		if(act->is_a_score &&
		   add_to_pattern(pat, ROLE_DO_SCORES | use_flags))
		  rflags |= ROLE_DO_SCORES;
		if(act->is_a_filter &&
		   add_to_pattern(pat, ROLE_DO_FILTER | use_flags))
		  rflags |= ROLE_DO_FILTER;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint((1,
		   "Trouble converting patterns to new variable\n"));
    }
}


/*
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_filts_patterns(long int use_flags)
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_FILT | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint((2, "converting old filter patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_filter &&
		   add_to_pattern(pat, ROLE_DO_FILTER | use_flags))
		  rflags |= ROLE_DO_FILTER;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint((1,
		   "Trouble converting filter patterns to new variable\n"));
    }
}


/*
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_scores_patterns(long int use_flags)
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_SCORE | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint((2, "converting old scores patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_score &&
		   add_to_pattern(pat, ROLE_DO_SCORES | use_flags))
		  rflags |= ROLE_DO_SCORES;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint((1,
		   "Trouble converting scores patterns to new variable\n"));
    }
}


/*
 * set_old_growth_bits - Command used to set or unset old growth set
 *			 of features
 */
void
set_old_growth_bits(struct pine *ps, int val)
{
    int i;

    for(i = 1; i <= F_FEATURE_LIST_COUNT; i++)
      if(test_old_growth_bits(ps, i))
	F_SET(i, ps, val);
}



/*
 * test_old_growth_bits - Test to see if all the old growth bits are on,
 *			  *or* if a particular feature index is in the old
 *			  growth set.
 *
 * WEIRD ALERT: if index == F_OLD_GROWTH bit values are tested
 *              otherwise a bits existence in the set is tested!!!
 *
 * BUG: this will break if an old growth feature number is ever >= 32.
 */
int
test_old_growth_bits(struct pine *ps, int index)
{
    /*
     * this list defines F_OLD_GROWTH set
     */
    static unsigned long old_growth_bits = ((1 << F_ENABLE_FULL_HDR)     |
					    (1 << F_ENABLE_PIPE)         |
					    (1 << F_ENABLE_TAB_COMPLETE) |
					    (1 << F_QUIT_WO_CONFIRM)     |
					    (1 << F_ENABLE_JUMP)         |
					    (1 << F_ENABLE_ALT_ED)       |
					    (1 << F_ENABLE_BOUNCE)       |
					    (1 << F_ENABLE_AGG_OPS)	 |
					    (1 << F_ENABLE_FLAG)         |
					    (1 << F_CAN_SUSPEND));
    if(index >= 32)
	return(0);

    if(index == F_OLD_GROWTH){
	for(index = 1; index <= F_FEATURE_LIST_COUNT; index++)
	  if(((1 << index) & old_growth_bits) && F_OFF(index, ps))
	    return(0);

	return(1);
    }
    else
      return((1 << index) & old_growth_bits);
}


/*
 * Side effect is that the appropriate global variable is set, and the
 * appropriate current_val is set.
 */
void
cur_rule_value(struct variable *var, int expand, int cmdline)
{
    int i;
    NAMEVAL_S *v;

    set_current_val(var, expand, cmdline);

    if(var == &ps_global->vars[V_SAVED_MSG_NAME_RULE]){
      if(ps_global->VAR_SAVED_MSG_NAME_RULE)
	for(i = 0; v = save_msg_rules(i); i++)
	  if(!strucmp(ps_global->VAR_SAVED_MSG_NAME_RULE, S_OR_L(v))){
	      ps_global->save_msg_rule = v->value;
	      break;
	  }
    }
#ifndef	_WINDOWS
    else if(var == &ps_global->vars[V_COLOR_STYLE]){
      if(ps_global->VAR_COLOR_STYLE)
	for(i = 0; v = col_style(i); i++)
	  if(!strucmp(ps_global->VAR_COLOR_STYLE, S_OR_L(v))){
	      ps_global->color_style = v->value;
	      break;
	  }
    }
#endif
    else if(var == &ps_global->vars[V_INDEX_COLOR_STYLE]){
      if(ps_global->VAR_INDEX_COLOR_STYLE)
	for(i = 0; v = index_col_style(i); i++)
	  if(!strucmp(ps_global->VAR_INDEX_COLOR_STYLE, S_OR_L(v))){
	      ps_global->index_color_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_TITLEBAR_COLOR_STYLE]){
      if(ps_global->VAR_TITLEBAR_COLOR_STYLE)
	for(i = 0; v = titlebar_col_style(i); i++)
	  if(!strucmp(ps_global->VAR_TITLEBAR_COLOR_STYLE, S_OR_L(v))){
	      ps_global->titlebar_color_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_FCC_RULE]){
      if(ps_global->VAR_FCC_RULE)
	for(i = 0; v = fcc_rules(i); i++)
	  if(!strucmp(ps_global->VAR_FCC_RULE, S_OR_L(v))){
	      ps_global->fcc_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_GOTO_DEFAULT_RULE]){
      if(ps_global->VAR_GOTO_DEFAULT_RULE)
	for(i = 0; v = goto_rules(i); i++)
	  if(!strucmp(ps_global->VAR_GOTO_DEFAULT_RULE, S_OR_L(v))){
	      ps_global->goto_default_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_INCOMING_STARTUP]){
      if(ps_global->VAR_INCOMING_STARTUP)
	for(i = 0; v = incoming_startup_rules(i); i++)
	  if(!strucmp(ps_global->VAR_INCOMING_STARTUP, S_OR_L(v))){
	      ps_global->inc_startup_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_PRUNING_RULE]){
      if(ps_global->VAR_PRUNING_RULE)
	for(i = 0; v = pruning_rules(i); i++)
	  if(!strucmp(ps_global->VAR_PRUNING_RULE, S_OR_L(v))){
	      ps_global->pruning_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_REOPEN_RULE]){
      if(ps_global->VAR_REOPEN_RULE)
	for(i = 0; v = reopen_rules(i); i++)
	  if(!strucmp(ps_global->VAR_REOPEN_RULE, S_OR_L(v))){
	      ps_global->reopen_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_FLD_SORT_RULE]){
      if(ps_global->VAR_FLD_SORT_RULE)
	for(i = 0; v = fld_sort_rules(i); i++)
	  if(!strucmp(ps_global->VAR_FLD_SORT_RULE, S_OR_L(v))){
	      ps_global->fld_sort_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_AB_SORT_RULE]){
      if(ps_global->VAR_AB_SORT_RULE)
	for(i = 0; v = ab_sort_rules(i); i++)
	  if(!strucmp(ps_global->VAR_AB_SORT_RULE, S_OR_L(v))){
	      ps_global->ab_sort_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_THREAD_DISP_STYLE]){
      if(ps_global->VAR_THREAD_DISP_STYLE)
	for(i = 0; v = thread_disp_styles(i); i++)
	  if(!strucmp(ps_global->VAR_THREAD_DISP_STYLE, S_OR_L(v))){
	      ps_global->thread_disp_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_THREAD_INDEX_STYLE]){
      if(ps_global->VAR_THREAD_INDEX_STYLE)
	for(i = 0; v = thread_index_styles(i); i++)
	  if(!strucmp(ps_global->VAR_THREAD_INDEX_STYLE, S_OR_L(v))){
	      ps_global->thread_index_style = v->value;
	      break;
	  }
    }
}


/*
 * Standard way to get at save message rules...
 */
NAMEVAL_S *
save_msg_rules(int index)
{
    static NAMEVAL_S save_rules[] = {
      {"by-from",			      NULL, SAV_RULE_FROM},
      {"by-nick-of-from",		      NULL, SAV_RULE_NICK_FROM_DEF},
      {"by-nick-of-from-then-from",	      NULL, SAV_RULE_NICK_FROM},
      {"by-fcc-of-from",		      NULL, SAV_RULE_FCC_FROM_DEF},
      {"by-fcc-of-from-then-from",	      NULL, SAV_RULE_FCC_FROM},
      {"by-realname-of-from",	 	      NULL, SAV_RULE_RN_FROM_DEF},
      {"by-realname-of-from-then-from",	      NULL, SAV_RULE_RN_FROM},
      {"by-sender",			      NULL, SAV_RULE_SENDER},
      {"by-nick-of-sender",		      NULL, SAV_RULE_NICK_SENDER_DEF},
      {"by-nick-of-sender-then-sender",	      NULL, SAV_RULE_NICK_SENDER},
      {"by-fcc-of-sender",		      NULL, SAV_RULE_FCC_SENDER_DEF},
      {"by-fcc-of-sender-then-sender",	      NULL, SAV_RULE_FCC_SENDER},
      {"by-realname-of-sender",		      NULL, SAV_RULE_RN_SENDER_DEF},
      {"by-realname-of-sender-then-sender",   NULL, SAV_RULE_RN_SENDER},
      {"by-recipient",			      NULL, SAV_RULE_RECIP},
      {"by-nick-of-recip",		      NULL, SAV_RULE_NICK_RECIP_DEF},
      {"by-nick-of-recip-then-recip",	      NULL, SAV_RULE_NICK_RECIP},
      {"by-fcc-of-recip",		      NULL, SAV_RULE_FCC_RECIP_DEF},
      {"by-fcc-of-recip-then-recip",	      NULL, SAV_RULE_FCC_RECIP},
      {"by-realname-of-recip",		      NULL, SAV_RULE_RN_RECIP_DEF},
      {"by-realname-of-recip-then-recip",     NULL, SAV_RULE_RN_RECIP},
      {"by-replyto",			      NULL, SAV_RULE_REPLYTO},
      {"by-nick-of-replyto",		      NULL, SAV_RULE_NICK_REPLYTO_DEF},
      {"by-nick-of-replyto-then-replyto",     NULL, SAV_RULE_NICK_REPLYTO},
      {"by-fcc-of-replyto",		      NULL, SAV_RULE_FCC_REPLYTO_DEF},
      {"by-fcc-of-replyto-then-replyto",      NULL, SAV_RULE_FCC_REPLYTO},
      {"by-realname-of-replyto",	      NULL, SAV_RULE_RN_REPLYTO_DEF},
      {"by-realname-of-replyto-then-replyto", NULL, SAV_RULE_RN_REPLYTO},
      {"last-folder-used",		      NULL, SAV_RULE_LAST}, 
      {"default-folder",		      NULL, SAV_RULE_DEFLT}
    };

    return((index >= 0 && index < (sizeof(save_rules)/sizeof(save_rules[0])))
	   ? &save_rules[index] : NULL);
}


/*
 * Standard way to get at fcc rules...
 */
NAMEVAL_S *
fcc_rules(int index)
{
    static NAMEVAL_S f_rules[] = {
	{"default-fcc",        NULL, FCC_RULE_DEFLT}, 
	{"last-fcc-used",      NULL, FCC_RULE_LAST}, 
	{"by-recipient",       NULL, FCC_RULE_RECIP},
	{"by-nickname",        NULL, FCC_RULE_NICK},
	{"by-nick-then-recip", NULL, FCC_RULE_NICK_RECIP},
	{"current-folder",     NULL, FCC_RULE_CURRENT}
    };

    return((index >= 0 && index < (sizeof(f_rules)/sizeof(f_rules[0])))
	   ? &f_rules[index] : NULL);
}


/*
 * Standard way to get at addrbook sort rules...
 */
NAMEVAL_S *
ab_sort_rules(int index)
{
    static NAMEVAL_S ab_rules[] = {
	{"fullname-with-lists-last",  NULL, AB_SORT_RULE_FULL_LISTS},
	{"fullname",                  NULL, AB_SORT_RULE_FULL}, 
	{"nickname-with-lists-last",  NULL, AB_SORT_RULE_NICK_LISTS},
	{"nickname",                  NULL, AB_SORT_RULE_NICK},
	{"dont-sort",                 NULL, AB_SORT_RULE_NONE}
    };

    return((index >= 0 && index < (sizeof(ab_rules)/sizeof(ab_rules[0])))
	   ? &ab_rules[index] : NULL);
}


/*
 * Standard way to get at color styles.
 */
NAMEVAL_S *
col_style(int index)
{
    static NAMEVAL_S col_styles[] = {
	{"no-color",			NULL, COL_NONE}, 
	{"use-termdef",			NULL, COL_TERMDEF},
	{"force-ansi-8color",		NULL, COL_ANSI8},
	{"force-ansi-16color",		NULL, COL_ANSI16},
	{"force-xterm-256color",	NULL, COL_ANSI256}
    };

    return((index >= 0 && index < (sizeof(col_styles)/sizeof(col_styles[0])))
	   ? &col_styles[index] : NULL);
}


/*
 * Standard way to get at index color styles.
 */
NAMEVAL_S *
index_col_style(int index)
{
    static NAMEVAL_S ind_col_styles[] = {
	{"flip-colors",			NULL, IND_COL_FLIP}, 
	{"reverse",			NULL, IND_COL_REV}, 
	{"reverse-fg",			NULL, IND_COL_FG},
	{"reverse-fg-no-ambiguity",	NULL, IND_COL_FG_NOAMBIG},
	{"reverse-bg",			NULL, IND_COL_BG},
	{"reverse-bg-no-ambiguity",	NULL, IND_COL_BG_NOAMBIG}
    };

    return((index >= 0 && index < (sizeof(ind_col_styles)/sizeof(ind_col_styles[0]))) ? &ind_col_styles[index] : NULL);
}


/*
 * Standard way to get at titlebar color styles.
 */
NAMEVAL_S *
titlebar_col_style(int index)
{
    static NAMEVAL_S tbar_col_styles[] = {
	{"default",			NULL, TBAR_COLOR_DEFAULT}, 
	{"indexline",			NULL, TBAR_COLOR_INDEXLINE}, 
	{"reverse-indexline",		NULL, TBAR_COLOR_REV_INDEXLINE}
    };

    return((index >= 0 && index < (sizeof(tbar_col_styles)/sizeof(tbar_col_styles[0]))) ? &tbar_col_styles[index] : NULL);
}


/*
 * Standard way to get at folder sort rules...
 */
NAMEVAL_S *
fld_sort_rules(int index)
{
    static NAMEVAL_S fdl_rules[] = {
	{"alphabetical",          NULL, FLD_SORT_ALPHA}, 
	{"alpha-with-dirs-last",  NULL, FLD_SORT_ALPHA_DIR_LAST},
	{"alpha-with-dirs-first", NULL, FLD_SORT_ALPHA_DIR_FIRST}
    };

    return((index >= 0 && index < (sizeof(fdl_rules)/sizeof(fdl_rules[0])))
	   ? &fdl_rules[index] : NULL);
}


/*
 * Standard way to get at incoming startup rules...
 */
NAMEVAL_S *
incoming_startup_rules(int index)
{
    static NAMEVAL_S is_rules[] = {
	{"first-unseen",		NULL, IS_FIRST_UNSEEN},
	{"first-recent",		NULL, IS_FIRST_RECENT}, 
	{"first-important",		NULL, IS_FIRST_IMPORTANT}, 
	{"first-important-or-unseen",	NULL, IS_FIRST_IMPORTANT_OR_UNSEEN}, 
	{"first-important-or-recent",	NULL, IS_FIRST_IMPORTANT_OR_RECENT}, 
	{"first",			NULL, IS_FIRST},
	{"last",			NULL, IS_LAST}
    };

    return((index >= 0 && index < (sizeof(is_rules)/sizeof(is_rules[0])))
	   ? &is_rules[index] : NULL);
}


NAMEVAL_S *
startup_rules(int index)
{
    static NAMEVAL_S is2_rules[] = {
	{"first-unseen",		NULL, IS_FIRST_UNSEEN},
	{"first-recent",		NULL, IS_FIRST_RECENT}, 
	{"first-important",		NULL, IS_FIRST_IMPORTANT}, 
	{"first-important-or-unseen",	NULL, IS_FIRST_IMPORTANT_OR_UNSEEN}, 
	{"first-important-or-recent",	NULL, IS_FIRST_IMPORTANT_OR_RECENT}, 
	{"first",			NULL, IS_FIRST},
	{"last",			NULL, IS_LAST},
	{"default",			NULL, IS_NOTSET}
    };

    return((index >= 0 && index < (sizeof(is2_rules)/sizeof(is2_rules[0])))
	   ? &is2_rules[index] : NULL);
}


/*
 * Standard way to get at pruning-rule values.
 */
NAMEVAL_S *
pruning_rules(int index)
{
    static NAMEVAL_S pr_rules[] = {
	{"ask about rename, ask about deleting","ask-ask", PRUNE_ASK_AND_ASK},
	{"ask about rename, don't delete",	"ask-no",  PRUNE_ASK_AND_NO},
	{"always rename, ask about deleting",	"yes-ask", PRUNE_YES_AND_ASK}, 
	{"always rename, don't delete",		"yes-no",  PRUNE_YES_AND_NO}, 
	{"don't rename, ask about deleting",	"no-ask",  PRUNE_NO_AND_ASK}, 
	{"don't rename, don't delete",		"no-no",   PRUNE_NO_AND_NO} 
    };

    return((index >= 0 && index < (sizeof(pr_rules)/sizeof(pr_rules[0])))
	   ? &pr_rules[index] : NULL);
}


/*
 * Standard way to get at reopen-rule values.
 */
NAMEVAL_S *
reopen_rules(int index)
{
    static NAMEVAL_S ro_rules[] = {
	/* TRANSLATORS: short description of a feature option */
	{"Always reopen",					"yes-yes",
							    REOPEN_YES_YES},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Yes for POP/NNTP, Ask about other remote [Yes]",	"yes-ask-y",
							    REOPEN_YES_ASK_Y},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Yes for POP/NNTP, Ask about other remote [No]",	"yes-ask-n",
							    REOPEN_YES_ASK_N},
	/* TRANSLATORS: short description of a feature option */
	{"Yes for POP/NNTP, No for other remote",		"yes-no",
							    REOPEN_YES_NO},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Always ask [Yes]",					"ask-ask-y",
							    REOPEN_ASK_ASK_Y},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Always ask [No]",					"ask-ask-n",
							    REOPEN_ASK_ASK_N},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Ask about POP/NNTP [Yes], No for other remote",	"ask-no-y",
							    REOPEN_ASK_NO_Y},
	/* TRANSLATORS: short description of a feature option, default in brackets */
	{"Ask about POP/NNTP [No], No for other remote",	"ask-no-n",
							    REOPEN_ASK_NO_N},
	/* TRANSLATORS: short description of a feature option */
	{"Never reopen",					"no-no",
							    REOPEN_NO_NO},
    };

    return((index >= 0 && index < (sizeof(ro_rules)/sizeof(ro_rules[0])))
	   ? &ro_rules[index] : NULL);
}


/*
 * Standard way to get at thread_disp_style values.
 */
NAMEVAL_S *
thread_disp_styles(int index)
{
    static NAMEVAL_S td_styles[] = {
	{"none",			"none",		THREAD_NONE},
	{"show-thread-structure",	"struct",	THREAD_STRUCT},
	{"mutt-like",			"mutt",		THREAD_MUTTLIKE},
	{"indent-subject-1",		"subj1",	THREAD_INDENT_SUBJ1},
	{"indent-subject-2",		"subj2",	THREAD_INDENT_SUBJ2},
	{"indent-from-1",		"from1",	THREAD_INDENT_FROM1},
	{"indent-from-2",		"from2",	THREAD_INDENT_FROM2},
	{"show-structure-in-from",	"struct-from",	THREAD_STRUCT_FROM}
    };

    return((index >= 0 && index < (sizeof(td_styles)/sizeof(td_styles[0])))
	   ? &td_styles[index] : NULL);
}


/*
 * Standard way to get at thread_index_style values.
 */
NAMEVAL_S *
thread_index_styles(int index)
{
    static NAMEVAL_S ti_styles[] = {
	{"regular-index-with-expanded-threads",	"exp",	THRDINDX_EXP},
	{"regular-index-with-collapsed-threads","coll",	THRDINDX_COLL},
	{"separate-index-screen-always",	"sep",	THRDINDX_SEP},
	{"separate-index-screen-except-for-single-messages","sep-auto",
							THRDINDX_SEP_AUTO}
    };

    return((index >= 0 && index < (sizeof(ti_styles)/sizeof(ti_styles[0])))
	   ? &ti_styles[index] : NULL);
}


/*
 * Standard way to get at goto default rules...
 */
NAMEVAL_S *
goto_rules(int index)
{
    static NAMEVAL_S g_rules[] = {
	{"folder-in-first-collection",		 NULL, GOTO_FIRST_CLCTN},
	{"inbox-or-folder-in-first-collection",	 NULL, GOTO_INBOX_FIRST_CLCTN},
	{"inbox-or-folder-in-recent-collection", NULL, GOTO_INBOX_RECENT_CLCTN},
	{"first-collection-with-inbox-default",	 NULL, GOTO_FIRST_CLCTN_DEF_INBOX},
	{"most-recent-folder",			 NULL, GOTO_LAST_FLDR}
    };

    return((index >= 0 && index < (sizeof(g_rules)/sizeof(g_rules[0])))
	   ? &g_rules[index] : NULL);
}


NAMEVAL_S *
pat_fldr_types(int index)
{
    static NAMEVAL_S pat_fldr_list[] = {
	{"Any",			"ANY",		FLDR_ANY},
	{"News",		"NEWS",		FLDR_NEWS},
	{"Email",		"EMAIL",	FLDR_EMAIL},
	{"Specific (Enter Incoming Nicknames or use ^T)", "SPEC", FLDR_SPECIFIC}
    };

    return((index >= 0 &&
	    index < (sizeof(pat_fldr_list)/sizeof(pat_fldr_list[0])))
		   ? &pat_fldr_list[index] : NULL);
}


NAMEVAL_S *
inabook_fldr_types(int indexarg)
{
    static NAMEVAL_S inabook_fldr_list[] = {
	{"Don't care, always matches",			"E",	IAB_EITHER},
	{"Yes, in any address book",			"YES",	IAB_YES},
	{"No, not in any address book",			"NO",	IAB_NO},
	{"Yes, in specific address books",		"SYES",	IAB_SPEC_YES},
	{"No, not in any of specific address books",	"SNO",	IAB_SPEC_NO}
    };

    int index = indexarg & IAB_TYPE_MASK;

    return((index >= 0 &&
	   index < (sizeof(inabook_fldr_list)/sizeof(inabook_fldr_list[0])))
		   ? &inabook_fldr_list[index] : NULL);
}


NAMEVAL_S *
filter_types(int index)
{
    static NAMEVAL_S filter_type_list[] = {
	{"Just Set Message Status",	"NONE",		FILTER_STATE},
	{"Delete",			"DEL",		FILTER_KILL},
	{"Move (Enter folder name(s) in primary collection, or use ^T)",
						    "FLDR", FILTER_FOLDER}
    };

    return((index >= 0 &&
	    index < (sizeof(filter_type_list)/sizeof(filter_type_list[0])))
		   ? &filter_type_list[index] : NULL);
}


NAMEVAL_S *
role_repl_types(int index)
{
    static NAMEVAL_S role_repl_list[] = {
	{"Never",			"NO",	ROLE_REPL_NO},
	{"With confirmation",		"YES",	ROLE_REPL_YES},
	{"Without confirmation",	"NC",	ROLE_REPL_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_repl_list)/sizeof(role_repl_list[0])))
		   ? &role_repl_list[index] : NULL);
}


NAMEVAL_S *
role_forw_types(int index)
{
    static NAMEVAL_S role_forw_list[] = {
	{"Never",		  	"NO",	ROLE_FORW_NO},
	{"With confirmation",		"YES",	ROLE_FORW_YES},
	{"Without confirmation",	"NC",	ROLE_FORW_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_forw_list)/sizeof(role_forw_list[0])))
		   ? &role_forw_list[index] : NULL);
}


NAMEVAL_S *
role_comp_types(int index)
{
    static NAMEVAL_S role_comp_list[] = {
	{"Never",		  	"NO",	ROLE_COMP_NO},
	{"With confirmation",		"YES",	ROLE_COMP_YES},
	{"Without confirmation",	"NC",	ROLE_COMP_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_comp_list)/sizeof(role_comp_list[0])))
		   ? &role_comp_list[index] : NULL);
}


NAMEVAL_S *
role_status_types(int index)
{
    static NAMEVAL_S role_status_list[] = {
	{"Don't care, always matches",	"E",	PAT_STAT_EITHER},
	{"Yes",				"YES",	PAT_STAT_YES},
	{"No",				"NO",	PAT_STAT_NO}
    };

    return((index >= 0 &&
	    index < (sizeof(role_status_list)/sizeof(role_status_list[0])))
		   ? &role_status_list[index] : NULL);
}


NAMEVAL_S *
msg_state_types(int index)
{
    static NAMEVAL_S msg_state_list[] = {
	{"Don't change it",		"LV",	ACT_STAT_LEAVE},
	{"Set this state",		"SET",	ACT_STAT_SET},
	{"Clear this state",		"CLR",	ACT_STAT_CLEAR}
    };

    return((index >= 0 &&
	    index < (sizeof(msg_state_list)/sizeof(msg_state_list[0])))
		   ? &msg_state_list[index] : NULL);
}


#ifdef	ENABLE_LDAP
NAMEVAL_S *
ldap_search_rules(int index)
{
    static NAMEVAL_S ldap_search_list[] = {
	{"contains",		NULL, LDAP_SRCH_CONTAINS},
	{"equals",		NULL, LDAP_SRCH_EQUALS},
	{"begins-with",		NULL, LDAP_SRCH_BEGINS},
	{"ends-with",		NULL, LDAP_SRCH_ENDS}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_search_list)/sizeof(ldap_search_list[0])))
		   ? &ldap_search_list[index] : NULL);
}


NAMEVAL_S *
ldap_search_types(int index)
{
    static NAMEVAL_S ldap_types_list[] = {
	{"name",				NULL, LDAP_TYPE_CN},
	{"surname",				NULL, LDAP_TYPE_SUR},
	{"givenname",				NULL, LDAP_TYPE_GIVEN},
	{"email",				NULL, LDAP_TYPE_EMAIL},
	{"name-or-email",			NULL, LDAP_TYPE_CN_EMAIL},
	{"surname-or-givenname",		NULL, LDAP_TYPE_SUR_GIVEN},
	{"sur-or-given-or-name-or-email",	NULL, LDAP_TYPE_SEVERAL}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_types_list)/sizeof(ldap_types_list[0])))
		   ? &ldap_types_list[index] : NULL);
}


NAMEVAL_S *
ldap_search_scope(int index)
{
    static NAMEVAL_S ldap_scope_list[] = {
	{"base",		NULL, LDAP_SCOPE_BASE},
	{"onelevel",		NULL, LDAP_SCOPE_ONELEVEL},
	{"subtree",		NULL, LDAP_SCOPE_SUBTREE}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_scope_list)/sizeof(ldap_scope_list[0])))
		   ? &ldap_scope_list[index] : NULL);
}
#endif


/*
 * Choose from the global default, command line args, pinerc values to set
 * the actual value of the variable that we will use.  Start at the top
 * and work down from higher to lower precedence.
 * For lists, we may inherit values from lower precedence
 * versions if that's the way the user specifies it.
 * The user can put INHERIT_DEFAULT as the first entry in a list and that
 * means it will inherit the current values, for example the values
 * from the global_val, or the value from the main_user_val could be
 * inherited in the post_user_val.
 */
void    
set_current_val(struct variable *var, int expand, int cmdline)
{
    int    is_set[5], is_inherit[5];
    int    i, j, k, cnt, start;
    char **tmp, **t, **list[5];
    char  *p;

    dprint((9,
	       "set_current_val(var=%s%s, expand=%d, cmdline=%d)\n",
	       (var && var->name) ? var->name : "?",
	       (var && var->is_list) ? " (list)" : "",
	       expand, cmdline));

    if(!var)
      return;

    if(var->is_list){			  /* variable is a list */

	for(j = 0; j < 5; j++){
	    t = j==0 ? var->global_val.l :
		j==1 ? var->main_user_val.l :
		j==2 ? var->post_user_val.l :
		j==3 ? ((cmdline) ? var->cmdline_val.l : NULL) :
		       var->fixed_val.l;

	    is_set[j] = is_inherit[j] = 0;
	    list[j] = NULL;

	    if(t){
		if(!expand){
		    is_set[j]++;
		    list[j] = t;
		}
		else{
		    for(i = 0; t[i]; i++){
			if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i],
					    0)){
			    /* successful expand */
			    is_set[j]++;
			    list[j] = t;
			    break;
			}
		    }
		}

		if(list[j] && list[j][0] && !strcmp(list[j][0],INHERIT))
		  is_inherit[j]++;
	    }
	}

	cnt = 0;
	start = 0;
	/* count how many items in current_val list */
	/* Admin wants default, which is global_val. */
	if(var->is_fixed && var->fixed_val.l == NULL){
	    cnt = 0;
	    if(is_set[0]){
		for(; list[0][cnt]; cnt++)
		  ;
	    }
	}
	else{
	    for(j = 0; j < 5; j++){
		if(is_set[j]){
		    if(!is_inherit[j]){
			cnt = 0;	/* reset */
			start = j;
		    }

		    for(i = is_inherit[j] ? 1 : 0; list[j][i]; i++)
		      cnt++;
		}
	    }
	}

	free_list_array(&var->current_val.l);	 /* clean up any old values */

	/* check to see if anything is set */
	if(is_set[0] + is_set[1] + is_set[2] + is_set[3] + is_set[4] > 0){
	    var->current_val.l = (char **)fs_get((cnt+1)*sizeof(char *));
	    tmp = var->current_val.l;
	    if(var->is_fixed && var->fixed_val.l == NULL){
		if(is_set[0]){
		    for(i = 0; list[0][i]; i++){
			if(!expand)
			  *tmp++ = cpystr(list[0][i]);
			else if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
						 list[0][i], 0))
			  *tmp++ = cpystr(tmp_20k_buf);
		    }
		}
	    }
	    else{
		for(j = start; j < 5; j++){
		    if(is_set[j]){
			for(i = is_inherit[j] ? 1 : 0; list[j][i]; i++){
			    if(!expand)
			      *tmp++ = cpystr(list[j][i]);
			    else if(expand_variables(tmp_20k_buf,SIZEOF_20KBUF,
						     list[j][i], 0))
			      *tmp++ = cpystr(tmp_20k_buf);
			}
		    }
		}
	    }

	    *tmp = NULL;
	}
	else
	  var->current_val.l = NULL;
    }
    else{  /* variable is not a list */
	char *strvar = NULL;

	for(j = 0; j < 5; j++){

	    p = j==0 ? var->fixed_val.p :
		j==1 ? ((cmdline) ? var->cmdline_val.p : NULL) :
		j==2 ? var->post_user_val.p :
		j==3 ? var->main_user_val.p :
		       var->global_val.p;

	    is_set[j] = 0;

	    if(p){
		if(!expand){
		    is_set[j]++;
		    if(!strvar)
			strvar = p;
		}
		else if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, p,
				(var == &ps_global->vars[V_MAILCAP_PATH] ||
				 var == &ps_global->vars[V_MIMETYPE_PATH]))){
		    is_set[j]++;
		    if(!strvar)
			strvar = p;
		}
	    }
	}

	/* Admin wants default, which is global_val. */
	if(var->is_fixed && var->fixed_val.p == NULL)
	  strvar = var->global_val.p;

	if(var->current_val.p)		/* free previous value */
	  fs_give((void **)&var->current_val.p);

	if(strvar){
	    if(!expand)
	      var->current_val.p = cpystr(strvar);
	    else{
		expand_variables(tmp_20k_buf, SIZEOF_20KBUF, strvar,
				 (var == &ps_global->vars[V_MAILCAP_PATH] ||
				  var == &ps_global->vars[V_MIMETYPE_PATH]));
		var->current_val.p = cpystr(tmp_20k_buf);
	    }
	}
	else
	  var->current_val.p = NULL;
    }

    if(var->is_fixed && !is_inherit[4]){
	char **flist;
	int fixed_len, user_len;

	/*
	 * sys mgr fixed this variable and user is trying to change it
	 */
	for(k = 1; !(ps_global->give_fixed_warning &&
		     ps_global->fix_fixed_warning) && k <= 3; k++){
	    if(is_set[k]){
		if(var->is_list){
		    t = k==1 ? ((cmdline) ? var->cmdline_val.l : NULL) :
			k==2 ? var->post_user_val.l :
			       var->main_user_val.l;

		    /* If same length and same contents, don't warn. */
		    for(flist=var->fixed_val.l; flist && *flist; flist++)
		      ;/* just counting */

		    fixed_len = var->fixed_val.l ? (flist - var->fixed_val.l)
						 : 0;
		    for(flist=t; flist && *flist; flist++)
		      ;/* just counting */

		    user_len = t ? (flist - t) : 0;
		    if(user_len == fixed_len){
		      for(i=0; i < user_len; i++){
			for(j=0; j < user_len; j++)
			  if(!strucmp(t[i], var->fixed_val.l[j]))
			    break;
			  
			if(j == user_len){
			  ps_global->give_fixed_warning = 1;
			  if(k != 1)
			    ps_global->fix_fixed_warning = 1;

			  break;
			}
		      }
		    }
		    else{
			ps_global->give_fixed_warning = 1;
			if(k != 1)
			  ps_global->fix_fixed_warning = 1;
		    }
		}
		else{
		    p = k==1 ? ((cmdline) ? var->cmdline_val.p : NULL) :
			k==2 ? var->post_user_val.p :
			       var->main_user_val.p;
		    
		    if((var->fixed_val.p && !p) ||
		       (!var->fixed_val.p && p) ||
		       (var->fixed_val.p && p && strucmp(var->fixed_val.p, p))){
			ps_global->give_fixed_warning = 1;
			if(k != 1)
			  ps_global->fix_fixed_warning = 1;
		    }
		}
	    }
	}
    }
}


void
set_news_spec_current_val(int expand, int cmdline)
{
    struct variable *newsvar = &ps_global->vars[V_NEWS_SPEC];
    struct variable *fvar    = &ps_global->vars[V_FOLDER_SPEC];

    /* check to see if it has a value */
    set_current_val(newsvar, expand, cmdline);

    /*
     * If no value, we might want to fake a value. We'll do that if
     * there is no news collection already defined in FOLDER_SPEC and if
     * there is also an NNTP_SERVER defined.
     */
    if(!newsvar->current_val.l && ps_global->VAR_NNTP_SERVER &&
       ps_global->VAR_NNTP_SERVER[0] && ps_global->VAR_NNTP_SERVER[0][0] &&
       !news_in_folders(fvar)){
	char buf[MAXPATH];

	newsvar->global_val.l = (char **)fs_get(2 * sizeof(char *));
	snprintf(buf, sizeof(buf), "{%.*s/nntp}#news.[]", sizeof(buf)-20,
		ps_global->VAR_NNTP_SERVER[0]);
	newsvar->global_val.l[0] = cpystr(buf);
	newsvar->global_val.l[1] = NULL;
	set_current_val(newsvar, expand, cmdline);
	/*
	 * But we're going to get rid of the fake global_val in case
	 * things change.
	 */
	free_list_array(&newsvar->global_val.l);
    }
}


/*
 * Feature-list has to be handled separately from the other variables
 * because it is additive.  The other variables choose one of command line,
 * or pine.conf, or pinerc.  Feature list adds them.  This could easily be
 * converted to a general purpose routine if we add more additive variables.
 *
 * This works by replacing earlier values with later ones.  That is, command
 * line settings have higher precedence than global settings and that is
 * accomplished by putting the command line features after the global
 * features in the list.  When they are processed, the last one wins.
 *
 * Feature-list also has a backwards compatibility hack.
 */
void    
set_feature_list_current_val(struct variable *var)
{
    char **list;
    char **list_fixed;
    char   no_allow[110], *allow;
    int    i, j, k, m,
	   elems = 0;

    elems++;	/* for default F_ALLOW_CHANGING_FROM */

    /* count the lists so we can allocate */
    for(m = 0; m < 6; m++){
	list = m==0 ? var->global_val.l :
		m==1 ? var->main_user_val.l :
		 m==2 ? var->post_user_val.l :
		  m==3 ? ps_global->feat_list_back_compat :
		   m==4 ? var->cmdline_val.l :
		           var->fixed_val.l;
	if(list)
	  for(i = 0; list[i]; i++)
	    elems++;
    }

    list_fixed = var->fixed_val.l;

    if(var->current_val.l)
      free_list_array(&var->current_val.l);

    var->current_val.l = (char **)fs_get((elems+1) * sizeof(char *));

    /*
     * We need to warn the user if the sys mgr has restricted him or her
     * from changing a feature that he or she is trying to change.
     *
     * We're not catching the old-growth macro since we're just comparing
     * strings. That is, it works correctly, but the user won't be warned
     * if the user old-growth and the mgr says no-quit-without-confirm.
     */

    j = 0;
    /* everything defaults to off except for allow-changing-from */
    allow = no_allow+3;
    strncpy(no_allow, "no-", 3);
    strncpy(allow, feature_list_name(F_ALLOW_CHANGING_FROM), 100);
    var->current_val.l[j++] = cpystr(allow);

    for(m = 0; m < 6; m++){
	list = m==0 ? var->global_val.l :
		m==1 ? var->main_user_val.l :
		 m==2 ? var->post_user_val.l :
		  m==3 ? ps_global->feat_list_back_compat :
		   m==4 ? var->cmdline_val.l :
		           var->fixed_val.l;
	if(list)
	  for(i = 0; list[i]; i++){
	      var->current_val.l[j++] = cpystr(list[i]);
	      if(m >= 1 && m <= 4){
		  for(k = 0; list_fixed && list_fixed[k]; k++){
		      char *p, *q;
		      p = list[i];
		      q = list_fixed[k];
		      if(!struncmp(p, "no-", 3))
			p += 3;
		      if(!struncmp(q, "no-", 3))
			q += 3;
		      if(!strucmp(q, p) && strucmp(list[i], list_fixed[k])){
			  ps_global->give_fixed_warning = 1;
			  if(m <= 2)
			    ps_global->fix_fixed_warning = 1;
		      }
		  }
	      }
	      else if(m == 5 && !strucmp(list[i], no_allow))
	        ps_global->never_allow_changing_from = 1;
	  }
    }

#ifdef	NEVER_ALLOW_CHANGING_FROM
    ps_global->never_allow_changing_from = 1;
#endif

    var->current_val.l[j] = NULL;
}
                                                     


/*----------------------------------------------------------------------

	Expand Metacharacters/variables in file-names

   Read input line and expand shell-variables/meta-characters

	<input>		<replaced by>
	$variable	getenv("variable")
	${variable}	getenv("variable")
	${variable:-defvalue} is getenv("variable") if variable is defined and
	                      is defvalue otherwise
	~		getenv("HOME")
	\c		c
	<others>	<just copied>

NOTE handling of braces in ${name} doesn't check much or do error recovery

   If colon_path is set, then we expand ~ not only at the start of linein,
   but also after each : in the path.
	
  ----*/

char *
expand_variables(char *lineout, size_t lineoutlen, char *linein, int colon_path)
{
    char *src = linein, *dest = lineout, *p;
    char *limit = lineout + lineoutlen;
    int   envexpand = 0;

    if(!linein)
      return(NULL);

    while(*src ){			/* something in input string */
        if(*src == '$' && *(src+1) == '$'){
	    /*
	     * $$ to escape chars we're interested in, else
	     * it's up to the user of the variable to handle the 
	     * backslash...
	     */
	    if(dest < limit)
              *dest++ = *++src;		/* copy next as is */
        }else
#if !(defined(DOS) || defined(OS2))
        if(*src == '\\' && *(src+1) == '$'){
	    /*
	     * backslash to escape chars we're interested in, else
	     * it's up to the user of the variable to handle the 
	     * backslash...
	     */
	    if(dest < limit)
              *dest++ = *++src;		/* copy next as is */
        }else if(*src == '~' &&
		 (src == linein || colon_path && *(src-1) == ':')){
	    char buf[MAXPATH];
	    int  i;

	    for(i = 0; i < sizeof(buf)-1 && src[i] && src[i] != '/'; i++)
	      buf[i] = src[i];

	    src    += i;		/* advance src pointer */
	    buf[i]  = '\0';		/* tie off buf string */
	    fnexpand(buf, sizeof(buf));	/* expand the path */

	    for(p = buf; dest < limit && (*dest = *p); p++, dest++)
	      ;

	    continue;
        }else
#endif
	if(*src == '$'){		/* shell variable */
	    char word[128+1], *colon = NULL, *rbrace = NULL;

	    envexpand++;		/* signal that we've expanded a var */
	    src++;			/* skip dollar */
	    if(*src == '{'){		/* starts with brace? */
		src++;        
		rbrace = strindex(src, '}');
		if(rbrace){
		    /* look for default value */
		    colon = strstr(src, ":-");
		    if(colon && (rbrace < colon))
		      colon = NULL;
		}
	    }

	    p = word;

	    /* put the env variable to be looked up in word */
	    if(rbrace){
		while(*src
		      && (p-word < sizeof(word)-1)
		      && ((colon && src < colon) || (!colon && src < rbrace))){
		    if(isspace((unsigned char) *src)){
			/*
			 * Illegal input. This should be an error of some
			 * sort but instead of that we'll just backup to the
			 * $ and treat it like it wasn't there.
			 */
			while(*src != '$')
			  src--;
			
			envexpand--;
			goto just_copy;
		    }
		    else
		      *p++ = *src++;
		}

		/* adjust src for next char */
		src = rbrace + 1;
	    }
	    else{
		while(*src && !isspace((unsigned char) *src)
		      && (p-word < sizeof(word)-1))
		  *p++ = *src++;
	    }

	    *p = '\0';

	    if((p = getenv(word)) != NULL){ /* check for word in environment */
		while(*p && dest < limit)
		  *dest++ = *p++;
	    }
	    else if(colon){		    /* else possible default value */
		p = colon + 2;
		while(*p && p < rbrace && dest < limit)
		  *dest++ = *p++;
	    }

	    continue;
	}else{				/* other cases: just copy */
just_copy:
	    if(dest < limit)
	      *dest++ = *src;
	}

        if(*src)			/* next character (if any) */
	  src++;
    }

    if(dest < limit)
      *dest = '\0';
    else
      lineout[lineoutlen-1] = '\0';

    return((envexpand && lineout[0] == '\0') ? NULL : lineout);
}


/*----------------------------------------------------------------------
    Sets  login, full_username and home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the fullname, login and home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/
#define	MAX_INIT_ERRS	10
void
init_error(struct pine *ps, int flags, int min_time, int max_time, char *message)
{
    int    i;

    if(!ps->init_errs){
	ps->init_errs = (INIT_ERR_S *)fs_get((MAX_INIT_ERRS + 1) *
					     sizeof(*ps->init_errs));
	memset(ps->init_errs, 0, (MAX_INIT_ERRS + 1) * sizeof(*ps->init_errs));
    }

    for(i = 0; i < MAX_INIT_ERRS; i++)
      if(!(ps->init_errs)[i].message){
	  (ps->init_errs)[i].message  = cpystr(message);
	  (ps->init_errs)[i].min_time = min_time;
	  (ps->init_errs)[i].max_time = max_time;
	  (ps->init_errs)[i].flags    = flags;
	  dprint((2, "%s\n", message ? message : "?"));
	  break;
      }
}


/*----------------------------------------------------------------------
         Read and parse a pinerc file
  
   Args:  Filename   -- name of the .pinerc file to open and read
          vars       -- The vars structure to store values in
          which_vars -- Whether the local or global values are being read

   Result: 

 This may be the local file or the global file.  The values found are
merged with the values currently in vars.  All values are strings and
are malloced; and existing values will be freed before the assignment.
Those that are <unset> will be left unset; their values will be NULL.
  ----*/
void
read_pinerc(PINERC_S *prc, struct variable *vars, ParsePinerc which_vars)
{
    char               *filename, *file, *value, **lvalue, *line, *error;
    char               *p, *p1, *free_file = NULL;
    struct variable    *v;
    PINERC_LINE        *pline = NULL;
    int                 line_count, was_quoted;
    int			i;

    if(!prc)
      return;

    dprint((2, "reading_pinerc \"%s\"\n",
	   prc->name ? prc->name : "?"));

    if(prc->type == Loc){
	filename = prc->name ? prc->name : "";
	file = free_file = read_file(filename, 0);

	/*
	 * This is questionable. In case the user edits the pinerc
	 * in Windows and adds a UTF-8 BOM, we skip it here. If the
	 * user adds a Unicode BOM we're in trouble. We could write it
	 * with the BOM ourselves but so far we leave it BOMless in
	 * order that it's the same on Unix and Windows.
	 */
	if(BOM_UTF8(file))
	  file += 3;
    }
    else{
	if((file = read_remote_pinerc(prc, which_vars)) != NULL)
	  ps_global->c_client_error[0] = '\0';

	free_file = file;
    }

    if(file == NULL || *file == '\0'){
#ifdef	DEBUG
	if(file == NULL){
          dprint((2, "Open failed: %s\n", error_description(errno)));
	}
	else{
	    if(prc->type == Loc){
	      dprint((1, "Read_pinerc: empty pinerc (new?)\n"));
	    }
	    else{
	      dprint((1, "Read_pinerc: new remote pinerc\n"));
	    }
	}
#endif /* DEBUG */

	if(which_vars == ParsePers){
	    /* problems getting remote config */
	    if(file == NULL && prc->type == RemImap){
		if(!pith_opt_remote_pinerc_failure
		   || !(*pith_opt_remote_pinerc_failure)())
		  exceptional_exit(_("Unable to read or write remote configuration"), -1);
	    }

	    ps_global->first_time_user = 1;
	    prc->outstanding_pinerc_changes = 1;
	}

        return;
    }
    else{
	if(prc->type == Loc &&
	   (which_vars == ParseFixed || which_vars == ParseGlobal ||
	    (can_access(filename, ACCESS_EXISTS) == 0 &&
	     can_access(filename, EDIT_ACCESS) != 0))){
	    prc->readonly = 1;
	    if(prc == ps_global->prc)
	      ps_global->readonly_pinerc = 1;
	}

	/*
	 * accept CRLF or LF newlines
	 */
	for(p = file; *p && *p != '\012'; p++)
	  ;

	if(p > file && *p && *(p-1) == '\015')	/* cvt crlf to lf */
	  for(p1 = p - 1; *p1 = *p; p++)
	    if(!(*p == '\015' && *(p+1) == '\012'))
	      p1++;
    }

    dprint((2, "Read %d characters:\n", strlen(file)));

    if(which_vars == ParsePers || which_vars == ParsePersPost){
	/*--- Count up lines and allocate structures */
	for(line_count = 0, p = file; *p != '\0'; p++)
          if(*p == '\n')
	    line_count++;

	prc->pinerc_lines = (PINERC_LINE *)
			       fs_get((3 + line_count) * sizeof(PINERC_LINE));
	memset((void *)prc->pinerc_lines, 0,
	       (3 + line_count) * sizeof(PINERC_LINE));
	pline = prc->pinerc_lines;
    }

    for(p = file, line = file; *p != '\0';){
        /*----- Grab the line ----*/
        line = p;
        while(*p && *p != '\n')
          p++;
        if(*p == '\n'){
            *p++ = '\0';
        }

        /*----- Comment Line -----*/
        if(*line == '#'){
	    /* no comments in remote pinercs */
            if(pline && prc->type == Loc){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	if(*line == '\0' || *line == '\t' || *line == ' '){
            p1 = line;
            while(*p1 == '\t' || *p1 == ' ')
               p1++;
            if(pline){
		/*
		 * This could be a continuation line from some future
		 * version of pine, or it could be a continuation line
		 * from a PC-Pine variable we don't know about in unix.
		 */
	        if(*p1 != '\0')
                    pline->line = cpystr(line);
	        else
                    pline->line = cpystr("");
               pline->is_var = 0;
               pline++;
            }
            continue;
	}

        /*----- look up matching 'v' and leave "value" after '=' ----*/
        for(v = vars; *line && v->name; v++)
	  if((i = strlen(v->name)) < strlen(line) && !struncmp(v->name,line,i)){
	      int j;

	      for(j = i; line[j] == ' ' || line[j] == '\t'; j++)
		;

	      if(line[j] == '='){	/* bingo! */
		  for(value = &line[j+1];
		      *value == ' ' || *value == '\t';
		      value++)
		    ;

		  break;
	      }
	      /* else either unrecognized var or bogus line */
	  }

        /*----- Didn't match any variable or bogus format -----*/
	/*
	 * This could be a variable from some future
	 * version of pine, or it could be a PC-Pine variable
	 * we don't know about in unix. Either way, we want to preserve
	 * it in the file.
	 */
        if(!v->name){
            if(pline){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	/*
	 * Previous versions have caused duplicate pinerc data to be
	 * written to pinerc files. This clause erases the duplicate
	 * information when we read it, and it will be removed from the file
	 * if we call write_pinerc. We test to see if the same variable
	 * appears later in the file, if so, we skip over it here.
	 * We don't care about duplicates if this isn't a pinerc we might
	 * write out, so include pline in the conditional.
	 * Note that we will leave all of the duplicate comments and blank
	 * lines in the file unless it is a remote pinerc. Luckily, the
	 * bug that caused the duplicates only applied to remote pinercs,
	 * so we should have that case covered.
	 *
	 * If we find a duplicate, we point p to the start
	 * of the next line that should be considered, and then skip back
	 * to the top of the loop.
	 */
	if(pline && var_is_in_rest_of_file(v->name, p)){
	    if(v->is_list)
	      p = skip_over_this_var(line, p);

	    continue;
	}

	
        /*----- Obsolete variable, read it anyway below, might use it -----*/
        if(v->is_obsolete){
            if(pline){
                pline->obsolete_var = 1;
                pline->line = cpystr(line);
                pline->var = v;
            }
        }

        /*----- Variable is in the list but unused for some reason -----*/
        if(!v->is_used){
            if(pline){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

        /*--- Var is not user controlled, leave it alone for back compat ---*/
        if(!v->is_user && pline){
	    pline->is_var = 0;
	    pline->line = cpystr(line);
	    pline++;
	    continue;
        }

	if(which_vars == ParseFixed)
	  v->is_fixed = 1;

        /*---- variable is unset, or it's global but expands to nothing ----*/
        if(!*value
	   || (which_vars == ParseGlobal
	       && !expand_variables(tmp_20k_buf, SIZEOF_20KBUF, value,
				    (v == &ps_global->vars[V_MAILCAP_PATH] ||
				     v == &ps_global->vars[V_MIMETYPE_PATH])))){
            if(v->is_user && pline){
                pline->is_var   = 1;
                pline->var = v;
                pline++;
            }
            continue;
        }

        /*--value is non-empty, store it handling quotes and trailing space--*/
        if(*value == '"' && !v->is_list && v->del_quotes){
            was_quoted = 1;
            value++;
            for(p1 = value; *p1 && *p1 != '"'; p1++);
            if(*p1 == '"')
              *p1 = '\0';
            else
              removing_trailing_white_space(value);
        }else
          was_quoted = 0;

	/*
	 * List Entry Parsing
	 *
	 * The idea is to parse a comma separated list of 
	 * elements, preserving quotes, and understanding
	 * continuation lines (that is ',' == "\n ").
	 * Quotes must be balanced within elements.  Space 
	 * within elements is preserved, but leading and trailing 
	 * space is trimmed.  This is a generic function, and it's 
	 * left to the the functions that use the lists to make sure
	 * they contain valid data...
	 */
	if(v->is_list){

	    was_quoted = 0;
	    line_count = 0;
	    p1         = value;
	    while(1){			/* generous count of list elements */
		if(*p1 == '"')		/* ignore ',' if quoted   */
		  was_quoted = (was_quoted) ? 0 : 1 ;

		if((*p1 == ',' && !was_quoted) || *p1 == '\n' || *p1 == '\0')
		  line_count++;		/* count this element */

		if(*p1 == '\0' || *p1 == '\n'){	/* deal with EOL */
		    if(p1 < p || *p1 == '\n'){
			*p1++ = ','; 	/* fix null or newline */

			if(*p1 != '\t' && *p1 != ' '){
			    *(p1-1) = '\0'; /* tie off list */
			    p       = p1;   /* reset p */
			    break;
			}
		    }else{
			p = p1;		/* end of pinerc */
			break;
		    }
		}else
		  p1++;
	    }

	    error  = NULL;
	    lvalue = parse_list(value, line_count,
				v->del_quotes ? PL_REMSURRQUOT : PL_NONE,
			        &error);
	    if(error){
		dprint((1,
		       "read_pinerc: ERROR: %s in %s = \"%s\"\n", 
			   error ? error : "?",
			   v->name ? v->name : "?",
			   value ? value : "?"));
	    }
	    /*
	     * Special case: turn "" strings into empty strings.
	     * This allows users to turn off default lists.  For example,
	     * if smtp-server is set then a user could override smtp-server
	     * with smtp-server="".
	     */
	    for(i = 0; lvalue[i]; i++)
		if(lvalue[i][0] == '"' &&
		   lvalue[i][1] == '"' &&
		   lvalue[i][2] == '\0')
		     lvalue[i][0] = '\0';
	}

        if(pline){
            if(v->is_user && (which_vars == ParsePers || !v->is_onlymain)){
		if(v->is_list){
		    char ***l;

		    l = (which_vars == ParsePers) ? &v->main_user_val.l
						  : &v->post_user_val.l;
		    free_list_array(l);
		    *l = lvalue;
		}
		else{
		    char **p;

		    p = (which_vars == ParsePers) ? &v->main_user_val.p
						  : &v->post_user_val.p;
		    if(p && *p != NULL)
		      fs_give((void **)p);

		    *p = cpystr(value);
		}

		if(pline){
		    pline->is_var    = 1;
		    pline->var       = v;
		    pline->is_quoted = was_quoted;
		    pline++;
		}
            }
        }
	else if(which_vars == ParseGlobal){
            if(v->is_global){
		if(v->is_list){
		    free_list_array(&v->global_val.l);
		    v->global_val.l = lvalue;
		}
		else{
		    if(v->global_val.p != NULL)
		      fs_give((void **) &(v->global_val.p));

		    v->global_val.p = cpystr(value);
		}
            }
        }
	else{  /* which_vars == ParseFixed */
            if(v->is_user || v->is_global){
		if(v->is_list){
		    free_list_array(&v->fixed_val.l);
		    v->fixed_val.l = lvalue;
		}
		else{
		    if(v->fixed_val.p != NULL)
		      fs_give((void **) &(v->fixed_val.p));

		    v->fixed_val.p = cpystr(value);
		}
	    }
	}

#ifdef DEBUG
	if(v->is_list){
	    char **l;
	    l =   (which_vars == ParsePers)     ? v->main_user_val.l :
	           (which_vars == ParsePersPost) ? v->post_user_val.l :
	            (which_vars == ParseGlobal)   ? v->global_val.l :
						     v->fixed_val.l;
	    if(l && *l && **l){
                dprint((5, " %20.20s : %s\n",
		       v->name ? v->name : "?",
		       *l ? *l : "?"));
	        while(++l && *l && **l)
                    dprint((5, " %20.20s : %s\n", "",
			   *l ? *l : "?"));
	    }
	}else{
	    char *p;
	    p =   (which_vars == ParsePers)     ? v->main_user_val.p :
	           (which_vars == ParsePersPost) ? v->post_user_val.p :
	            (which_vars == ParseGlobal)   ? v->global_val.p :
						     v->fixed_val.p;
	    if(p && *p)
                dprint((5, " %20.20s : %s\n",
		       v->name ? v->name : "?",
		       p ? p : "?"));
	}
#endif /* DEBUG */
    }

    if(pline){
        pline->line = NULL;
        pline->is_var = 0;
	if(!prc->pinerc_written && prc->type == Loc){
	    prc->pinerc_written = name_file_mtime(filename);
	    dprint((5, "read_pinerc: time_pinerc_written = %ld\n",
		       (long) prc->pinerc_written));
	}
    }

    if(free_file)
      fs_give((void **) &free_file);
}


/*
 * Args   varname   The variable name we're looking for
 *        begin     Begin looking here
 *
 * Returns 1   if variable varname appears in the rest of the file
 *         0   if not
 */
int
var_is_in_rest_of_file(char *varname, char *begin)
{
    char *p;

    if(!(varname && *varname && begin && *begin))
      return 0;

    p = begin;

    while(p = srchstr(p, varname)){
	/* beginning of a line? */
	if(p > begin && (*(p-1) != '\n' && *(p-1) != '\r')){
	    p++;
	    continue;
	}

	/* followed by [ SPACE ] < = > ? */
	p += strlen(varname);
	while(*p == ' ' || *p == '\t')
	  p++;
	
	if(*p == '=')
	  return 1;
    }
    
    return 0;
}


/*
 * Args   begin    Variable to skip starts here.
 *        nextline This is where the next line starts. We need to know this
 *                 because the input has been mangled a little. A \0 has
 *                 replaced the \n at the end of the first line, but we can
 *                 use nextline to help us out of that quandry.
 *
 * Return a pointer to the start of the first line after this variable
 *        and all of its continuation lines.
 */
char *
skip_over_this_var(char *begin, char *nextline)
{
    char *p;

    p = begin;

    while(1){
	if(*p == '\0' || *p == '\n'){		/* EOL */
	    if(p < nextline || *p == '\n'){	/* there may be another line */
		p++;
		if(*p != ' ' && *p != '\t')	/* no continuation line */
		  return(p);
	    }
	    else				/* end of file */
	      return(p);
	}
	else
	  p++;
    }
}


static char quotes[3] = {'"', '"', '\0'};
/*----------------------------------------------------------------------
    Write out the .pinerc state information

   Args: ps -- The pine structure to take state to be written from
      which -- Which pinerc to write
      flags -- If bit WRP_NOUSER is set, then assume that there is
                not a user present to answer questions.

  This writes to a temporary file first, and then renames that to 
 be the new .pinerc file to protect against disk error.  This has the 
 problem of possibly messing up file protections, ownership and links.
  ----*/
write_pinerc(struct pine *ps, EditWhich which, int flags)
{
    char               *p, *dir, *tmp = NULL, *pinrc;
    char               *pval, **lval;
    int                 bc = 1;
    FILE               *f;
    PINERC_LINE        *pline;
    struct variable    *var;
    time_t		mtime;
    char               *filename;
    REMDATA_S          *rd = NULL;
    PINERC_S           *prc = NULL;
    STORE_S            *so = NULL;

    dprint((2,"---- write_pinerc(%s) ----\n",
	    (which == Main) ? "Main" : "Post"));

    switch(which){
      case Main:
	prc = ps ? ps->prc : NULL;
	break;
      case Post:
	prc = ps ? ps->post_prc : NULL;
	break;
    }

    if(!prc)
      return(-1);
    
    if(prc->quit_to_edit){
	if(!(flags & WRP_NOUSER))
	  quit_to_edit_msg(prc);

	return(-1);
    }

    if(prc->type != Loc && !prc->readonly){

	bc = 0;			/* don't do backcompat conversion */
	rd = prc->rd;
	if(!rd)
	  return(-1);
	
	rd_check_remvalid(rd, -10L);

	if(rd->flags & REM_OUTOFDATE){
	    if((flags & WRP_NOUSER) || unexpected_pinerc_change()){
		prc->outstanding_pinerc_changes = 1;
		if(!(flags & WRP_NOUSER))
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Pinerc \"%.200s\" NOT saved",
				    prc->name ? prc->name : "");
		dprint((2, "write_pinerc: remote pinerc changed\n"));
		return(-1);
	    }
	    else
	      rd->flags &= ~REM_OUTOFDATE;
	}

	rd_open_remote(rd);

	if(rd->access == ReadWrite){
	    int ro;

	    if((ro=rd_remote_is_readonly(rd)) || rd->flags & REM_OUTOFDATE){
		if(ro == 1){
		    if(!(flags & WRP_NOUSER))
		      q_status_message(SM_ORDER | SM_DING, 5, 15,
			     _("Can't access remote config, changes NOT saved!"));
		    dprint((1,
	    "write_pinerc: Can't write to remote pinerc %s, aborting write\n",
			   rd->rn ? rd->rn : "?"));
		}
		else if(ro == 2){
		    if(!(rd->flags & NO_META_UPDATE)){
			unsigned long save_chk_nmsgs;

			switch(rd->type){
			  case RemImap:
			    save_chk_nmsgs = rd->t.i.chk_nmsgs;
			    rd->t.i.chk_nmsgs = 0;
			    rd_write_metadata(rd, 0);
			    rd->t.i.chk_nmsgs = save_chk_nmsgs;
			    break;

			  default:
			    q_status_message(SM_ORDER | SM_DING, 3, 5,
					 "Write_pinerc: Type not supported");
			    break;
			}
		    }

		    if(!(flags & WRP_NOUSER))
		      q_status_message1(SM_ORDER | SM_DING, 5, 15,
	    _("No write permission for remote config %.200s, changes NOT saved!"),
				    rd->rn);
		}
		else{
		    if(!(flags & WRP_NOUSER))
		      q_status_message(SM_ORDER | SM_DING, 5, 15,
		 _("Remote config changed, aborting our change to avoid damage..."));
		    dprint((1,
			    "write_pinerc: remote config %s changed since we started pine, aborting write\n",
			    prc->name ? prc->name : "?"));
		}

		rd->flags &= ~DO_REMTRIM;
		return(-1);
	    }

	    filename = rd->lf;
	}
	else{
	    prc->readonly = 1;
	    if(prc == ps->prc)
	      ps->readonly_pinerc = 1;
	}
    }
    else
      filename = prc->name ? prc->name : "";

    pinrc = prc->name ? prc->name : "";

    if(prc->type == Loc){
	mtime = name_file_mtime(filename);
	if(prc->pinerc_written
	   && prc->pinerc_written != mtime
	   && ((flags & WRP_NOUSER) || unexpected_pinerc_change())){
	    prc->outstanding_pinerc_changes = 1;

	    if(!(flags & WRP_NOUSER))
	      q_status_message1(SM_ORDER | SM_DING, 3, 3,
				"Pinerc \"%.200s\" NOT saved", pinrc);

	    dprint((2,"write_pinerc: mtime mismatch: \"%s\": %ld != %ld\n",
		    filename ? filename : "?",
		    (long) prc->pinerc_written, (long) mtime));
	    return(-1);
	}
    }

    /* don't write if pinerc is read-only */
    if(prc->readonly ||
         (filename &&
	  can_access(filename, ACCESS_EXISTS) == 0 &&
          can_access(filename, EDIT_ACCESS) != 0)){
	prc->readonly = 1;
	if(prc == ps->prc)
	  ps->readonly_pinerc = 1;

	if(!(flags & WRP_NOUSER))
	  q_status_message1(SM_ORDER | SM_DING, 0, 5,
		      _("Can't modify configuration file \"%.200s\": ReadOnly"),
			    pinrc);
	dprint((2, "write_pinerc: fail because can't access pinerc\n"));

	if(rd)
	  rd->flags &= ~DO_REMTRIM;

	return(-1);
    }

    if(rd && rd->flags & NO_FILE){
	so = rd->sonofile;
	so_truncate(rd->sonofile, 0L);		/* reset storage object */
    }
    else{
	dir = ".";
	if(p = last_cmpnt(filename)){
	    *--p = '\0';
	    dir = filename;
	}

#if	defined(DOS) || defined(OS2)
	if(!(isalpha((unsigned char)dir[0]) && dir[1] == ':' && dir[2] == '\0')
	   && (can_access(dir, EDIT_ACCESS) < 0 &&
	       our_mkdir(dir, 0700) < 0))
	{
	    if(!(flags & WRP_NOUSER))
	      q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      /* TRANSLATORS: first argument is a filename, second
			         arg is the text of the error message */
			      _("Error creating \"%.200s\" : %.200s"), dir,
			      error_description(errno));
	    if(rd)
	      rd->flags &= ~DO_REMTRIM;

	    return(-1);
	}

	tmp = temp_nam(dir, "rc", 0);

	if(*dir && tmp && !in_dir(dir, tmp)){
	    our_unlink(tmp);
	    fs_give((void **)&tmp);
	}

	if(p)
	  *p = '\\';

	if(tmp == NULL)
	  goto io_err;

#else  /* !DOS */
	tmp = temp_nam((*dir) ? dir : "/", "pinerc", 0);

	/*
	 * If temp_nam can't write in dir it puts the temp file in a
	 * temp directory, which won't help us when we go to rename.
	 */
	if(*dir && tmp && !in_dir(dir, tmp)){
	    our_unlink(tmp);
	    fs_give((void **)&tmp);
	}

	if(p)
	  *p = '/';

	if(tmp == NULL)
	  goto io_err;

#endif  /* !DOS */

	if((so = so_get(FileStar, tmp, WRITE_ACCESS)) == NULL)
	  goto io_err;
    }

    for(var = ps->vars; var->name != NULL; var++) 
      var->been_written = 0;

    if(prc->type == Loc && ps->first_time_user &&
       !so_puts(so, native_nl(cf_text_comment)))
      goto io_err;

    /* Write out what was in the .pinerc */
    for(pline = prc->pinerc_lines;
	pline && (pline->is_var || pline->line); pline++){
	if(pline->is_var){
	    var = pline->var;

	    if(var->is_list)
	      lval = LVAL(var, which);
	    else
	      pval = PVAL(var, which);

	    /* variable is not set */
	    if((var->is_list && (!lval || !lval[0])) ||
	       (!var->is_list && !pval)){
		/* leave null variables out of remote pinerc */
		if(prc->type == Loc &&
		   (!so_puts(so, var->name) || !so_puts(so, "=") ||
		    !so_puts(so, NEWLINE)))
		  goto io_err;
	    }
	    /* var is set to empty string */
	    else if((var->is_list && lval[0][0] == '\0') ||
		    (!var->is_list && pval[0] == '\0')){
		if(!so_puts(so, var->name) || !so_puts(so, "=") ||
		   !so_puts(so, quotes) || !so_puts(so, NEWLINE))
		  goto io_err;
	    }
	    else{
		if(var->is_list){
		    int i = 0;

		    for(i = 0; lval[i]; i++){
			snprintf(tmp_20k_buf, 10000, "%s%s%s%s%s",
				(i) ? "\t" : var->name,
				(i) ? "" : "=",
				lval[i][0] ? lval[i] : quotes,
				lval[i+1] ? "," : "", NEWLINE);
			tmp_20k_buf[10000-1] = '\0';
			if(!so_puts(so, bc ? backcompat_convert_from_utf8(tmp_20k_buf+10000, SIZEOF_20KBUF-10000, tmp_20k_buf) : tmp_20k_buf))
			  goto io_err;
		    }
		}
		else{
		    snprintf(tmp_20k_buf, 10000, "%s=%s%s%s%s",
			    var->name,
			    (pline->is_quoted && pval[0] != '\"')
			      ? "\"" : "",
			    pval,
			    (pline->is_quoted && pval[0] != '\"')
			      ? "\"" : "", NEWLINE);
		    tmp_20k_buf[10000-1] = '\0';
		    if(!so_puts(so, bc ? backcompat_convert_from_utf8(tmp_20k_buf+10000, SIZEOF_20KBUF-10000, tmp_20k_buf) : tmp_20k_buf))
		      goto io_err;
		}
	    }

	    var->been_written = 1;

	}else{
	    /*
	     * The description text should be changed into a message
	     * about the variable being obsolete when a variable is
	     * moved to obsolete status.  We add that message before
	     * the variable unless it is already there.  However, we
	     * leave the variable itself in case the user runs an old
	     * version of pine again.  Note that we have read in the
	     * value of the variable in read_pinerc and translated it
	     * into a new variable if appropriate.
	     */
	    if(pline->obsolete_var && prc->type == Loc){
		if(pline <= prc->pinerc_lines || (pline-1)->line == NULL ||
		   strlen((pline-1)->line) < 3 ||
		   strucmp((pline-1)->line+2, pline->var->descrip) != 0)
		  if(!so_puts(so, "# ") ||
		     !so_puts(so, native_nl(pline->var->descrip)) ||
		     !so_puts(so, NEWLINE))
		    goto io_err;
	    }

	    /* remove comments from remote pinercs */
	    if((prc->type == Loc ||
		(pline->line[0] != '#' && pline->line[0] != '\0')) &&
	        (!so_puts(so, pline->line) || !so_puts(so, NEWLINE)))
	      goto io_err;
	}
    }

    /* Now write out all the variables not in the .pinerc */
    for(var = ps->vars; var->name != NULL; var++){
	if(!var->is_user || var->been_written || !var->is_used ||
	   var->is_obsolete || (var->is_onlymain && which != Main))
	  continue;

	if(var->is_list)
	  lval = LVAL(var, which);
	else
	  pval = PVAL(var, which);

	/*
	 * set description to NULL to eliminate preceding
	 * blank and comment line.
	 */
	if(prc->type == Loc && var->descrip && *var->descrip &&
	   (!so_puts(so, NEWLINE) || !so_puts(so, "# ") ||
	    !so_puts(so, native_nl(var->descrip)) || !so_puts(so, NEWLINE)))
	  goto io_err;

	/* variable is not set */
	/** Don't know what the global_val thing is for. SH, Mar 00 **/
	if((var->is_list && (!lval || (!lval[0] && !var->global_val.l))) ||
	   (!var->is_list && !pval)){
	    /* leave null variables out of remote pinerc */
	    if(prc->type == Loc &&
	       (!so_puts(so, var->name) || !so_puts(so, "=") ||
	        !so_puts(so, NEWLINE)))
	      goto io_err;
	}
	/* var is set to empty string */
	else if((var->is_list && (!lval[0] || !lval[0][0]))
		|| (!var->is_list && pval[0] == '\0')){
	    if(!so_puts(so, var->name) || !so_puts(so, "=") ||
	       !so_puts(so, quotes) || !so_puts(so, NEWLINE))
	      goto io_err;
	}
	else if(var->is_list){
	    int i = 0;

	    for(i = 0; lval[i] ; i++){
		snprintf(tmp_20k_buf, 10000, "%s%s%s%s%s",
			(i) ? "\t" : var->name,
			(i) ? "" : "=",
			lval[i],
			lval[i+1] ? "," : "", NEWLINE);
		tmp_20k_buf[10000-1] = '\0';
		if(!so_puts(so, bc ? backcompat_convert_from_utf8(tmp_20k_buf+10000, SIZEOF_20KBUF-10000, tmp_20k_buf) : tmp_20k_buf))
		  goto io_err;
	    }
	}
	else{
	    char *pconverted;

	    pconverted = bc ? backcompat_convert_from_utf8(tmp_20k_buf, SIZEOF_20KBUF, pval) : pval;

	    if(!so_puts(so, var->name) || !so_puts(so, "=") ||
	       !so_puts(so, pconverted) || !so_puts(so, NEWLINE))
	      goto io_err;
	}
    }

    if(!(rd && rd->flags & NO_FILE)){
	if(so_give(&so))
	  goto io_err;

	file_attrib_copy(tmp, filename);
	if(rename_file(tmp, filename) < 0)
	  goto io_err;
    }
    
    if(prc->type != Loc){
	int   e, we_cancel;
	char datebuf[200];

	datebuf[0] = '\0';

	if(!(flags & WRP_NOUSER))
	  we_cancel = busy_cue(_("Copying to remote config"), NULL, 1);

	if((e = rd_update_remote(rd, datebuf)) != 0){
	    dprint((1,
		   "write_pinerc: error copying from %s to %s\n",
		   rd->lf ? rd->lf : "<memory>", rd->rn ? rd->rn : "?"));
	    if(!(flags & WRP_NOUSER)){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				_("Error copying to %.200s: %.200s"),
				rd->rn, error_description(errno));
		
		q_status_message(SM_ORDER | SM_DING, 5, 5,
       _("Copy of config to remote folder failed, changes NOT saved remotely"));
	    }
	}
	else{
	    rd_update_metadata(rd, datebuf);
	    rd->read_status = 'W';
	    rd_trim_remdata(&rd);
	    rd_close_remote(rd);
	}

	if(we_cancel)
	  cancel_busy_cue(-1);
    }

    prc->outstanding_pinerc_changes = 0;

    if(prc->type == Loc){
	prc->pinerc_written = name_file_mtime(filename);
	dprint((2, "wrote pinerc: %s: time_pinerc_written = %ld\n",
		   pinrc ? pinrc : "?", (long) prc->pinerc_written));
    }
    else{
	dprint((2, "wrote pinerc: %s\n", pinrc ? pinrc : "?"));
    }

    if(tmp){
	our_unlink(tmp);
	fs_give((void **)&tmp);
    }

    return(0);

  io_err:
    if(!(flags & WRP_NOUSER))
      q_status_message2(SM_ORDER | SM_DING, 3, 5,
		        _("Error saving configuration in \"%.200s\": %.200s"),
		        pinrc, error_description(errno));

    dprint((1, "Error writing %s : %s\n", pinrc ? pinrc : "?",
	       error_description(errno)));
    if(rd)
      rd->flags &= ~DO_REMTRIM;
    if(tmp){
	our_unlink(tmp);
	fs_give((void **)&tmp);
    }

    return(-1);
}


/*
 * The srcstr is UTF-8. In order to help the user with
 * running this pine and an old pre-alpine pine on the same config
 * file we attempt to convert the values of the config variables
 * to the user's character set before writing.
 */
char *
backcompat_convert_from_utf8(char *buf, size_t buflen, char *srcstr)
{
    char *converted = NULL;
    char *p;
    int its_ascii = 1;


    for(p = srcstr; *p && its_ascii; p++)
      if(*p & 0x80)
	its_ascii = 0;

    /* if it is ascii, go with that */
    if(its_ascii)
      converted = srcstr;
    else{
	char *trythischarset = NULL;

	/*
	 * If it is possible to translate the UTF-8
	 * string into the user's character set then
	 * do that. For backwards compatibility with
	 * old pines.
	 */
	if(ps_global->keyboard_charmap && ps_global->keyboard_charmap[0])
	  trythischarset = ps_global->keyboard_charmap;
	else if(ps_global->display_charmap && ps_global->display_charmap[0])
	  trythischarset = ps_global->display_charmap;

	if(trythischarset){
	    SIZEDTEXT src, dst;

	    src.data = (unsigned char *) srcstr;
	    src.size = strlen(srcstr);
	    memset(&dst, 0, sizeof(dst));
	    if(utf8_cstext(&src, trythischarset, &dst, 0)){
		if(dst.data){
		    strncpy(buf, (char *) dst.data, buflen);
		    buf[buflen-1] = '\0';
		    fs_give((void **) &dst.data);
		    converted = buf;
		}
	    }
	}

	if(!converted)
	  converted = srcstr;
    }

    return(converted);
}


/*
 * Given a unix-style source string which may contain LFs,
 * convert those to CRLFs if appropriate.
 *
 * Returns a pointer to the converted string. This will be a string
 * stored in tmp_20k_buf.
 *
 * This is just used for the variable descriptions in the pinerc file. It
 * could certainly be fancier. It simply converts all \n to NEWLINE.
 */
char *
native_nl(char *src)
{ 
    char *q, *p;

    tmp_20k_buf[0] = '\0';

    if(src){
	for(q = (char *)tmp_20k_buf; *src; src++){
	    if(*src == '\n'){
		for(p = NEWLINE; *p; p++)
		  *q++ = *p;
	    }
	    else
	      *q++ = *src;
	}

	*q = '\0';
    }

    return((char *)tmp_20k_buf);
}


void
quit_to_edit_msg(PINERC_S *prc)
{
    /* TRANSLATORS: The %s is either "Postload " or nothing. A Postload config file
       is a type of config file. */
    q_status_message1(SM_ORDER, 3, 4, _("Must quit Alpine to change %sconfig file."),
		      (prc == ps_global->post_prc) ? "Postload " : "");
}


/*------------------------------------------------------------
  Return TRUE if the given string was a feature name present in the
  pinerc as it was when pine was started...
  ----*/
var_in_pinerc(char *s)
{
    PINERC_LINE *pline;

    for(pline = ps_global->prc ? ps_global->prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->var && pline->var->name && !strucmp(s, pline->var->name))
	return(1);

    for(pline = ps_global->post_prc ? ps_global->post_prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->var && pline->var->name && !strucmp(s, pline->var->name))
	return(1);

    return(0);
}


/*------------------------------------------------------------
  Free resources associated with pinerc_lines data
 ----*/
void
free_pinerc_lines(PINERC_LINE **pinerc_lines)
{
    PINERC_LINE *pline;

    if(pinerc_lines && *pinerc_lines){
	for(pline = *pinerc_lines; pline->var || pline->line; pline++)
	  if(pline->line)
	    fs_give((void **)&pline->line);

	fs_give((void **)pinerc_lines);
    }
}


/*------------------------------------------------------------
    Dump out a global pine.conf on the standard output with fresh
    comments.  Preserves variables currently set in SYSTEM_PINERC, if any.
  ----*/
void
dump_global_conf(void)
{
     FILE            *f;
     struct variable *var;
     PINERC_S        *prc;

     prc = new_pinerc_s(SYSTEM_PINERC);
     read_pinerc(prc, variables, ParseGlobal);
     if(prc)
       free_pinerc_s(&prc);

     f = stdout;
     if(f == NULL) 
       goto io_err;

     fprintf(f, "#      %s -- system wide pine configuration\n#\n",
	     SYSTEM_PINERC);
     fprintf(f, "# Values here affect all pine users unless they've overridden the values\n");
     fprintf(f, "# in their .pinerc files.  A copy of this file with current comments may\n");
     fprintf(f, "# be obtained by running \"pine -conf\". It will be printed to standard output.\n#\n");
     fprintf(f,"# For a variable to be unset its value must be null/blank.  This is not the\n");
     fprintf(f,"# same as the value of \"empty string\", which can be used to effectively\n");
     fprintf(f,"# \"unset\" a variable that has a default or previously assigned value.\n");
     fprintf(f,"# To set a variable to the empty string its value should be \"\".\n");
     fprintf(f,"# Switch variables are set to either \"yes\" or \"no\", and default to \"no\".\n");
     fprintf(f,"# Except for feature-list items, which are additive, values set in the\n");
     fprintf(f,"# .pinerc file replace those in pine.conf, and those in pine.conf.fixed\n");
     fprintf(f,"# over-ride all others.  Features can be over-ridden in .pinerc or\n");
     fprintf(f,"# pine.conf.fixed by pre-pending the feature name with \"no-\".\n#\n");
     fprintf(f,"# (These comments are automatically inserted.)\n");

     for(var = variables; var->name != NULL; var++){
         if(!var->is_global || !var->is_used || var->is_obsolete)
           continue;

         if(var->descrip && *var->descrip){
           if(fprintf(f, "\n# %s\n", var->descrip) == EOF)
             goto io_err;
	 }

	 if(var->is_list){
	     if(var->global_val.l == NULL){
		 if(fprintf(f, "%s=\n", var->name) == EOF)
		   goto io_err;
	     }else{
		 int i;

		 for(i=0; var->global_val.l[i]; i++)
		   if(fprintf(f, "%s%s%s%s\n", (i) ? "\t" : var->name,
			      (i) ? "" : "=", var->global_val.l[i],
			      var->global_val.l[i+1] ? ",":"") == EOF)
		     goto io_err;
	     }
	 }else{
	     if(var->global_val.p == NULL){
		 if(fprintf(f, "%s=\n", var->name) == EOF)
		   goto io_err;
	     }else if(strlen(var->global_val.p) == 0){
		 if(fprintf(f, "%s=\"\"\n", var->name) == EOF)
               goto io_err;
	     }else{
		 if(fprintf(f,"%s=%s\n",var->name,var->global_val.p) == EOF)
		   goto io_err;
	     }
	 }
     }
     exit(0);


   io_err:
     fprintf(stderr, "Error writing config to stdout: %s\n",
             error_description(errno));
     exit(-1);
}


/*------------------------------------------------------------
    Dump out a pinerc to filename with fresh
    comments.  Preserves variables currently set in pinerc, if any.
  ----*/
void
dump_new_pinerc(char *filename)
{
    FILE            *f;
    struct variable *var;
    char             buf[MAXPATH], *p;
    PINERC_S        *prc;


    p = ps_global->pinerc;

#if defined(DOS) || defined(OS2)
    if(!ps_global->pinerc){
	char *p;
	int   l;

	if(p = getenv("PINERC")){
	    ps_global->pinerc = cpystr(p);
	}else{
	    char buf2[MAXPATH];
	    build_path(buf2, ps_global->home_dir, DF_PINEDIR, sizeof(buf2));
	    build_path(buf, buf2, SYSTEM_PINERC, sizeof(buf));
	}

	p = buf;
    }
#else	/* !DOS */
    if(!ps_global->pinerc){
	build_path(buf, ps_global->home_dir, ".pinerc", sizeof(buf));
	p = buf;
    }
#endif	/* !DOS */

    prc = new_pinerc_s(p);
    read_pinerc(prc, variables, ParsePers);
    if(prc)
      free_pinerc_s(&prc);

    f = NULL;;
    if(filename[0] == '\0'){
	fprintf(stderr, "Missing argument to \"-pinerc\".\n");
    }else if(!strcmp(filename, "-")){
	f = stdout;
    }else{
	f = our_fopen(filename, "wb");
    }

    if(f == NULL) 
	goto io_err;

    if(fprintf(f, "%s", cf_text_comment) == EOF)
	goto io_err;

    for(var = variables; var->name != NULL; var++){
	dprint((7,"write_pinerc: %s = %s\n",
	       var->name ? var->name : "?",
	       var->main_user_val.p ? var->main_user_val.p : "<not set>"));
        if(!var->is_user || !var->is_used || var->is_obsolete)
	    continue;

	/*
	 * set description to NULL to eliminate preceding
	 * blank and comment line.
	 */
         if(var->descrip && *var->descrip){
           if(fprintf(f, "\n# %s\n", var->descrip) == EOF)
             goto io_err;
	 }

	if(var->is_list){
	    if(var->main_user_val.l == NULL){
		if(fprintf(f, "%s=\n", var->name) == EOF)
		    goto io_err;
	    }else{
		int i;

		for(i=0; var->main_user_val.l[i]; i++)
		    if(fprintf(f, "%s%s%s%s\n", (i) ? "\t" : var->name,
			      (i) ? "" : "=", var->main_user_val.l[i],
			      var->main_user_val.l[i+1] ? ",":"") == EOF)
		    goto io_err;
	    }
	}else{
	    if(var->main_user_val.p == NULL){
		if(fprintf(f, "%s=\n", var->name) == EOF)
		    goto io_err;
	    }else if(strlen(var->main_user_val.p) == 0){
		if(fprintf(f, "%s=\"\"\n", var->name) == EOF)
		    goto io_err;
	    }else{
		if(fprintf(f,"%s=%s\n",var->name,var->main_user_val.p) == EOF)
		    goto io_err;
	    }
	}
    }
    exit(0);


io_err:
    snprintf(buf, sizeof(buf), "Error writing config to %s: %s\n",
	    filename, error_description(errno));
    exceptional_exit(buf, -1);
}


/*----------------------------------------------------------------------
      Set a user variable and save the .pinerc
   
  Args:  var -- The index of the variable to set from conftype.h (V_....)
         value -- The string to set the value to

 Result: -1 is returned on failure and 0 is returned on success

 The vars data structure is updated and the pinerc saved.
 ----*/ 
set_variable(int var, char *value, int expand, int commit, EditWhich which)
{
    struct variable *v;
    char           **apval;
    PINERC_S        *prc;

    v = &ps_global->vars[var];

    if(!v->is_user) 
      panic1("Trying to set non-user variable %s", v->name);
    
    /* Override value of which, at most one of these should be set */
    if(v->is_onlymain)
      which = Main;
    else if(v->is_outermost)
      which = ps_global->ew_for_except_vars;

    apval = APVAL(v, which);

    if(!apval)
      return(-1);

    if(*apval)
      fs_give((void **)apval);

    *apval = value ? cpystr(value) : NULL;
    set_current_val(v, expand, FALSE);

    switch(which){
      case Main:
	prc = ps_global->prc;
	break;
      case Post:
	prc = ps_global->post_prc;
	break;
    }

    if(prc)
      prc->outstanding_pinerc_changes = 1;

    return(commit ? write_pinerc(ps_global, which, WRP_NONE) : 1);
}


/*----------------------------------------------------------------------
      Set a user variable list and save the .pinerc
   
  Args:  var -- The index of the variable to set from conftype.h (V_....)
         lvalue -- The list to set the value to

 Result: -1 is returned on failure and 0 is returned on success

 The vars data structure is updated and if write_it, the pinerc is saved.
 ----*/ 
set_variable_list(int var, char **lvalue, int write_it, EditWhich which)
{
    char          ***alval;
    int              i;
    struct variable *v = &ps_global->vars[var];
    PINERC_S        *prc;

    if(!v->is_user || !v->is_list)
      panic1("BOTCH: Trying to set non-user or non-list variable %s", v->name);

    /* Override value of which, at most one of these should be set */
    if(v->is_onlymain)
      which = Main;
    else if(v->is_outermost)
      which = ps_global->ew_for_except_vars;

    alval = ALVAL(v, which);
    if(!alval)
      return(-1);

    if(*alval)
      free_list_array(alval);

    if(lvalue){
	for(i = 0; lvalue[i] ; i++)	/* count elements */
	  ;

	*alval = (char **) fs_get((i+1) * sizeof(char *));

	for(i = 0; lvalue[i] ; i++)
	  (*alval)[i] = cpystr(lvalue[i]);

	(*alval)[i]         = NULL;
    }

    set_current_val(v, TRUE, FALSE);

    switch(which){
      case Main:
	prc = ps_global->prc;
	break;
      case Post:
	prc = ps_global->post_prc;
	break;
    }

    if(prc)
      prc->outstanding_pinerc_changes = 1;

    return(write_it ? write_pinerc(ps_global, which, WRP_NONE) : 0);
}
           

void
set_current_color_vals(struct pine *ps)
{
    struct variable *vars = ps->vars;
    int later_color_is_set = 0;

    set_current_val(&vars[V_NORM_FORE_COLOR], TRUE, TRUE);
    set_current_val(&vars[V_NORM_BACK_COLOR], TRUE, TRUE);
    pico_nfcolor(VAR_NORM_FORE_COLOR);
    pico_nbcolor(VAR_NORM_BACK_COLOR);

    set_current_val(&vars[V_REV_FORE_COLOR], TRUE, TRUE);
    set_current_val(&vars[V_REV_BACK_COLOR], TRUE, TRUE);
    pico_rfcolor(VAR_REV_FORE_COLOR);
    pico_rbcolor(VAR_REV_BACK_COLOR);

    set_color_val(&vars[V_TITLE_FORE_COLOR], 1);
    set_color_val(&vars[V_TITLECLOSED_FORE_COLOR], 0);
    set_color_val(&vars[V_STATUS_FORE_COLOR], 1);
    set_color_val(&vars[V_KEYLABEL_FORE_COLOR], 1);
    set_color_val(&vars[V_KEYNAME_FORE_COLOR], 1);
    set_color_val(&vars[V_SLCTBL_FORE_COLOR], 1);
    set_color_val(&vars[V_METAMSG_FORE_COLOR], 1);
    set_color_val(&vars[V_PROMPT_FORE_COLOR], 1);
    set_color_val(&vars[V_IND_PLUS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_IMP_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_DEL_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_ANS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_NEW_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_REC_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_UNS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_ARR_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_OP_FORE_COLOR], 0);
    set_color_val(&vars[V_SIGNATURE_FORE_COLOR], 0);

    set_current_val(&ps->vars[V_VIEW_HDR_COLORS], TRUE, TRUE);
    set_current_val(&ps->vars[V_KW_COLORS], TRUE, TRUE);
    set_custom_spec_colors(ps);

    /*
     * Set up the quoting colors. If a later color is set but not an earlier
     * color we set the earlier color to Normal to make it easier when
     * we go to use the colors. However, if the only quote colors set are
     * Normal that is the same as no settings, so delete them.
     */
    set_color_val(&vars[V_QUOTE1_FORE_COLOR], 0);
    set_color_val(&vars[V_QUOTE2_FORE_COLOR], 0);
    set_color_val(&vars[V_QUOTE3_FORE_COLOR], 0);

    if((!(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE3_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE3_BACK_COLOR, VAR_NORM_BACK_COLOR)))   &&
       (!(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE2_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE2_BACK_COLOR, VAR_NORM_BACK_COLOR)))   &&
       (!(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE1_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE1_BACK_COLOR, VAR_NORM_BACK_COLOR)))){
	/*
	 * They are all either Normal or not set. Delete them all.
	 */
	if(VAR_QUOTE3_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE3_FORE_COLOR);
	if(VAR_QUOTE3_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE3_BACK_COLOR);
	if(VAR_QUOTE2_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE2_FORE_COLOR);
	if(VAR_QUOTE2_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE2_BACK_COLOR);
	if(VAR_QUOTE1_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE1_FORE_COLOR);
	if(VAR_QUOTE1_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE1_BACK_COLOR);
    }
    else{			/* something is non-Normal */
	if(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR)
	  later_color_is_set++;

	/* if 3 is set but not 2, set 2 to Normal */
	if(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR)
	  later_color_is_set++;
	else if(later_color_is_set)
	  set_color_val(&vars[V_QUOTE2_FORE_COLOR], 1);

	/* if 3 or 2 is set but not 1, set 1 to Normal */
	if(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR)
	  later_color_is_set++;
	else if(later_color_is_set)
	  set_color_val(&vars[V_QUOTE1_FORE_COLOR], 1);
    }

#ifdef	_WINDOWS
    if(ps->pre441){
	int conv_main = 0, conv_post = 0;

	ps->pre441 = 0;
	if(ps->prc && !unix_color_style_in_pinerc(ps->prc)){
	    conv_main = convert_pc_gray_names(ps, ps->prc, Main);
	    if(conv_main)
	      ps->prc->outstanding_pinerc_changes = 1;
	}
	

	if(ps->post_prc && !unix_color_style_in_pinerc(ps->post_prc)){
	    conv_post = convert_pc_gray_names(ps, ps->post_prc, Post);
	    if(conv_post)
	      ps->post_prc->outstanding_pinerc_changes = 1;
	}
	
	if(conv_main || conv_post){
	    if(conv_main)
	      write_pinerc(ps, Main, WRP_NONE);

	    if(conv_post)
	      write_pinerc(ps, Post, WRP_NONE);

	    set_current_color_vals(ps);
	}
    }
#endif	/* _WINDOWS */

    pico_set_normal_color();
}


/*
 * Set current_val for the foreground and background color vars, which
 * are assumed to be in order. If a set_current_val on them doesn't
 * produce current_vals, then use the colors from defvar to set those
 * current_vals.
 */
void
set_color_val(struct variable *v, int use_default)
{
    set_current_val(v, TRUE, TRUE);
    set_current_val(v+1, TRUE, TRUE);

    if(!(v->current_val.p && v->current_val.p[0] &&
         (v+1)->current_val.p && (v+1)->current_val.p[0])){
	struct variable *defvar;

	if(v->current_val.p)
	  fs_give((void **)&v->current_val.p);
	if((v+1)->current_val.p)
	  fs_give((void **)&(v+1)->current_val.p);

	if(!use_default)
	  return;

	if(var_defaults_to_rev(v))
	  defvar = &ps_global->vars[V_REV_FORE_COLOR];
	else
	  defvar = &ps_global->vars[V_NORM_FORE_COLOR];

	/* use default vars values instead */
	if(defvar && defvar->current_val.p && defvar->current_val.p[0] &&
           (defvar+1)->current_val.p && (defvar+1)->current_val.p[0]){
	    v->current_val.p = cpystr(defvar->current_val.p);
	    (v+1)->current_val.p = cpystr((defvar+1)->current_val.p);
	}
    }
}


int
var_defaults_to_rev(struct variable *v)
{
    return(v == &ps_global->vars[V_REV_FORE_COLOR] ||
	   v == &ps_global->vars[V_TITLE_FORE_COLOR] ||
	   v == &ps_global->vars[V_STATUS_FORE_COLOR] ||
	   v == &ps_global->vars[V_KEYNAME_FORE_COLOR] ||
	   v == &ps_global->vars[V_PROMPT_FORE_COLOR]);
}



/*
 * Each item in the list looks like:
 *
 *  /HDR=<header>/FG=<foreground color>/BG=<background color>
 *
 * We separate the three pieces into an array of structures to make
 * it easier to deal with later.
 */
void
set_custom_spec_colors(struct pine *ps)
{
    if(ps->hdr_colors)
      free_spec_colors(&ps->hdr_colors);

    ps->hdr_colors = spec_colors_from_varlist(ps->VAR_VIEW_HDR_COLORS, 1);

    /* fit keyword colors into the same structures for code re-use */
    if(ps->kw_colors)
      free_spec_colors(&ps->kw_colors);

    ps->kw_colors = spec_colors_from_varlist(ps->VAR_KW_COLORS, 1);
}


/*
 * Input is one item from config variable.
 *
 * Return value must be freed by caller. The return is a single SPEC_COLOR_S,
 * not a list.
 */
SPEC_COLOR_S *
spec_color_from_var(char *t, int already_expanded)
{
    char        *p, *spec, *fg, *bg;
    PATTERN_S   *val;
    SPEC_COLOR_S *new_hcolor = NULL;

    if(t && t[0] && !strcmp(t, INHERIT)){
	new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	new_hcolor->inherit = 1;
    }
    else if(t && t[0]){
	char tbuf[10000];

	if(!already_expanded){
	    tbuf[0] = '\0';
	    if(expand_variables(tbuf, sizeof(tbuf), t, 0))
	      t = tbuf;
	}

	spec = fg = bg = NULL;
	val = NULL;
	if((p = srchstr(t, "/HDR=")) != NULL)
	  spec = remove_backslash_escapes(p+5);
	if((p = srchstr(t, "/FG=")) != NULL)
	  fg = remove_backslash_escapes(p+4);
	if((p = srchstr(t, "/BG=")) != NULL)
	  bg = remove_backslash_escapes(p+4);
	val = parse_pattern("VAL", t, 0);
	
	if(spec && *spec){
	    /* remove colons */
	    if((p = strindex(spec, ':')) != NULL)
	      *p = '\0';

	    new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	    memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	    new_hcolor->spec = spec;
	    new_hcolor->fg   = fg;
	    new_hcolor->bg   = bg;
	    new_hcolor->val  = val;
	}
	else{
	    if(spec)
	      fs_give((void **)&spec);
	    if(fg)
	      fs_give((void **)&fg);
	    if(bg)
	      fs_give((void **)&bg);
	    if(val)
	      free_pattern(&val);
	}
    }

    return(new_hcolor);
}


/*
 * Input is a list from config file.
 *
 * Return value may be a list of SPEC_COLOR_S and must be freed by caller.
 */
SPEC_COLOR_S *
spec_colors_from_varlist(char **varlist, int already_expanded)
{
    char        **s, *t;
    SPEC_COLOR_S *new_hc = NULL;
    SPEC_COLOR_S *new_hcolor, **nexthc;

    nexthc = &new_hc;
    if(varlist){
	for(s = varlist; (t = *s) != NULL; s++){
	    if(t[0]){
		new_hcolor = spec_color_from_var(t, already_expanded);
		if(new_hcolor){
		    *nexthc = new_hcolor;
		    nexthc = &new_hcolor->next;
		}
	    }
	}
    }

    return(new_hc);
}


/*
 * Returns allocated charstar suitable for config var for a single
 * SPEC_COLOR_S.
 */
char *
var_from_spec_color(SPEC_COLOR_S *hc)
{
    char *ret_val = NULL;
    char *p, *spec = NULL, *fg = NULL, *bg = NULL, *val = NULL;
    size_t len;

    if(hc && hc->inherit)
      ret_val = cpystr(INHERIT);
    else if(hc){
	if(hc->spec)
	  spec = add_viewerhdr_escapes(hc->spec);
	if(hc->fg)
	  fg = add_viewerhdr_escapes(hc->fg);
	if(hc->bg)
	  bg = add_viewerhdr_escapes(hc->bg);
	if(hc->val){
	    p = pattern_to_string(hc->val);
	    if(p){
		val = add_viewerhdr_escapes(p);
		fs_give((void **)&p);
	    }
	}

	len = strlen("/HDR=/FG=/BG=") + strlen(spec ? spec : "") +
	      strlen(fg ? fg : "") + strlen(bg ? bg : "") +
	      strlen(val ? "/VAL=" : "") + strlen(val ? val : "");
	ret_val = (char *) fs_get(len + 1);
	snprintf(ret_val, len+1, "/HDR=%s/FG=%s/BG=%s%s%s",
		spec ? spec : "", fg ? fg : "", bg ? bg : "",
		val ? "/VAL=" : "", val ? val : "");

	if(spec)
	  fs_give((void **)&spec);
	if(fg)
	  fs_give((void **)&fg);
	if(bg)
	  fs_give((void **)&bg);
	if(val)
	  fs_give((void **)&val);
    }

    return(ret_val);
}


/*
 * Returns allocated charstar suitable for config var for a single
 * SPEC_COLOR_S.
 */
char **
varlist_from_spec_colors(SPEC_COLOR_S *hcolors)
{
    SPEC_COLOR_S *hc;
    char       **ret_val = NULL;
    int          i;

    /* count how many */
    for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
      ;
    
    ret_val = (char **)fs_get((i+1) * sizeof(*ret_val));
    memset((void *)ret_val, 0, (i+1) * sizeof(*ret_val));
    for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
      ret_val[i] = var_from_spec_color(hc);
    
    return(ret_val);
}


void
update_posting_charset(struct pine *ps, int revert)
{
    if(F_ON(F_USE_SYSTEM_TRANS, ps)){
	if(!revert)
	  q_status_message(SM_ORDER, 5, 5, _("This change has no effect because feature Use-System-Translation is on"));
    }
    else{
	if(ps->posting_charmap)
	  fs_give((void **) &ps->posting_charmap);

	if(ps->VAR_POST_CHAR_SET){
	    ps->posting_charmap = cpystr(ps->VAR_POST_CHAR_SET);
	    if(!posting_charset_is_supported(ps->posting_charmap)){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			 _("Posting-Character set \"%s\" is unsupported, using UTF-8"),
			 ps->posting_charmap);
		q_status_message(SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
		fs_give((void **) &ps->posting_charmap);
		ps->posting_charmap = cpystr("UTF-8");
	    }
	}
	else
	  ps->posting_charmap = cpystr("UTF-8");
    }
}


int
feature_gets_an_x(struct pine *ps, struct variable *var, FEATURE_S *feature,
		  char **comment, EditWhich ew)
{
    char           **lval, **lvalexc, **lvalnorm;
    char            *def = "  (default)";
    int              j, done = 0;
    int              feature_fixed_on = 0, feature_fixed_off = 0;

    if(comment)
      *comment = NULL;

    lval  = LVAL(var, ew);
    lvalexc  = LVAL(var, ps_global->ew_for_except_vars);
    lvalnorm = LVAL(var, Main);
  
    /* feature value is administratively fixed */
    if(j = feature_in_list(var->fixed_val.l, feature->name)){
	if(j == 1)
	  feature_fixed_on++;
	else if(j == -1)
	  feature_fixed_off++;

	done++;
	if(comment)
	  *comment = "  (fixed)";
    }

    /*
     * We have an exceptions config setting which overrides anything
     * we do here, in the normal config.
     */
    if(!done &&
       ps_global->ew_for_except_vars != Main && ew == Main &&
       feature_in_list(lvalexc, feature->name)){
	done++;
	if(comment)
	  *comment = "  (overridden)";
    }

    /*
     * Feature is set On in default but not set here.
     */
    if(!done &&
       !feature_in_list(lval, feature->name) &&
       ((feature_in_list(var->global_val.l, feature->name) == 1) ||
        ((ps_global->ew_for_except_vars != Main &&
          ew == ps_global->ew_for_except_vars &&
          feature_in_list(lvalnorm, feature->name) == 1)))){
	done = 17;
	if(comment)
	  *comment = def;
    }

    /*
     * Feature allow-changing-from is on by default.
     * Tests say it is not in the list we're editing, and,
     * is not in the global_val list, and,
     * if we're editing an except which is not the normal then it is also
     * not in the normal list.
     * So it isn't set anywhere which means it is on because of the special
     * default value for this feature.
     */
    if(!done &&
       feature->id == F_ALLOW_CHANGING_FROM &&
       !feature_in_list(lval, feature->name) &&
       !feature_in_list(var->global_val.l, feature->name) &&
       (ps_global->ew_for_except_vars == Main ||
        ew != ps_global->ew_for_except_vars ||
        !feature_in_list(lvalnorm, feature->name))){
	done = 17;
	if(comment)
	  *comment = def;
    }

    return(feature_fixed_on ||
	   (!feature_fixed_off &&
	    (done == 17 ||
	     test_feature(lval, feature->name,
			  test_old_growth_bits(ps, feature->id)))));
}


void
toggle_feature(struct pine *ps, struct variable *var, FEATURE_S *f,
	       int just_flip_value, EditWhich ew)
{
    char      **vp, *p, **lval, ***alval;
    int		og, on_before, was_set;
    char       *err;
    long	l;

    og = test_old_growth_bits(ps, f->id);

    /*
     * if this feature is in the fixed set, or old-growth is in the fixed
     * set and this feature is in the old-growth set, don't alter it...
     */
    for(vp = var->fixed_val.l; vp && *vp; vp++){
	p = (struncmp(*vp, "no-", 3)) ? *vp : *vp + 3;
	if(!strucmp(p, f->name) || (og && !strucmp(p, "old-growth"))){
	    q_status_message(SM_ORDER, 3, 3,
	    /* TRANSLATORS: In the configuration screen, telling the user we
	       can't change this option because the system administrator
	       prohibits it. */
			     _("Can't change value fixed by sys-admin."));
	    return;
	}
    }

    on_before = F_ON(f->id, ps);

    lval  = LVAL(var, ew);
    alval = ALVAL(var, ew);
    if(just_flip_value)
      was_set = test_feature(lval, f->name, og);
    else
      was_set = feature_gets_an_x(ps, var, f, NULL, ew);

    if(alval)
      set_feature(alval, f->name, !was_set);

    set_feature_list_current_val(var);
    process_feature_list(ps, var->current_val.l, 0, 0, 0);
    
    /*
     * Handle any features that need special attention here...
     */
    if(on_before != F_ON(f->id, ps))
     switch(f->id){
      case F_QUOTE_ALL_FROMS :
	mail_parameters(NULL,SET_FROMWIDGET,F_ON(f->id,ps) ? VOIDT : NIL);
	break;

      case F_FAKE_NEW_IN_NEWS :
        if(IS_NEWS(ps->mail_stream))
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "news-approximates-new-status won't affect current newsgroup until next open");

	break;

      case F_COLOR_LINE_IMPORTANT :
	clear_index_cache(ps->mail_stream, 0);
	break;

      case F_DISABLE_INDEX_LOCALE_DATES :
	reset_index_format();
	clear_index_cache(ps->mail_stream, 0);
	break;

      case F_DISABLE_INPUT_HISTORY :
	if(F_ON(f->id,ps)){
	    free_histlist();
	}

	break;

      case F_MARK_FOR_CC :
	clear_index_cache(ps->mail_stream, 0);
	if(THREADING() && sp_viewing_a_thread(ps->mail_stream))
	  unview_thread(ps, ps->mail_stream, ps->msgmap);

	break;

      case F_HIDE_NNTP_PATH :
	mail_parameters(NULL, SET_NNTPHIDEPATH,
			F_ON(f->id, ps) ? VOIDT : NIL);
	break;

      case F_MAILDROPS_PRESERVE_STATE :
	mail_parameters(NULL, SET_SNARFPRESERVE,
			F_ON(f->id, ps) ? VOIDT : NIL);
	break;

      case F_DISABLE_SHARED_NAMESPACES :
	mail_parameters(NULL, SET_DISABLEAUTOSHAREDNS,
			F_ON(f->id, ps) ? VOIDT : NIL);
	break;

      case F_QUELL_LOCK_FAILURE_MSGS :
	mail_parameters(NULL, SET_LOCKEACCESERROR,
			F_ON(f->id, ps) ? VOIDT : NIL);
	break;

      case F_MULNEWSRC_HOSTNAMES_AS_TYPED :
	l = F_ON(f->id, ps) ? 0L : 1L;
	mail_parameters(NULL, SET_NEWSRCCANONHOST, (void *) l);
	break;

      case F_QUELL_INTERNAL_MSG :
	mail_parameters(NULL, SET_USERHASNOLIFE,
			F_ON(f->id, ps) ? VOIDT : NIL);
	break;

      case F_DISABLE_SETLOCALE_COLLATE :
	set_collation(F_OFF(F_DISABLE_SETLOCALE_COLLATE, ps), 1);
	break;

      case F_USE_SYSTEM_TRANS :
	err = NULL;
	reset_character_set_stuff(&err);
	if(err){
	    q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	    fs_give((void **) &err);
	}

	break;

      case F_ENABLE_INCOMING_UNSEEN :
	if(!on_before && F_OFF(F_ENABLE_INCOMING, ps))
	  q_status_message(SM_ORDER, 0, 3, _("This option has no effect without Enable-Incoming-Folders"));
	break;

      default :
	break;
     }
}


/*
 * Returns 1 -- Feature is in the list and positive
 *         0 -- Feature is not in the list at all
 *        -1 -- Feature is in the list and negative (no-)
 */
int
feature_in_list(char **l, char *f)
{
    char *p;
    int   rv = 0, forced_off;

    for(; l && *l; l++){
	p = (forced_off = !struncmp(*l, "no-", 3)) ? *l + 3 : *l;
	if(!strucmp(p, f))
	  rv = forced_off ? -1 : 1;
    }

    return(rv);
}


/*
 * test_feature - runs thru a feature list, and returns:
 *                 1 if feature explicitly set and matches 'v'
 *                 0 if feature not explicitly set *or* doesn't match 'v'
 */
int
test_feature(char **l, char *f, int g)
{
    char *p;
    int   rv = 0, forced_off;

    for(; l && *l; l++){
	p = (forced_off = !struncmp(*l, "no-", 3)) ? *l + 3 : *l;
	if(!strucmp(p, f))
	  rv = !forced_off;
	else if(g && !strucmp(p, "old-growth"))
	  rv = !forced_off;
    }

    return(rv);
}


void
set_feature(char ***l, char *f, int v)
{
    char **list = l ? *l : NULL, newval[256];
    int    count = 0;

    snprintf(newval, sizeof(newval), "%s%s", v ? "" : "no-", f);
    for(; list && *list; list++, count++)
      if((**list == '\0')                       /* anything can replace an empty value */
	 || !strucmp(((!struncmp(*list, "no-", 3)) ? *list + 3 : *list), f)){
	  fs_give((void **)list);		/* replace with new value */
	  *list = cpystr(newval);
	  return;
      }

    /*
     * if we got here, we didn't find it in the list, so grow the list
     * and add it..
     */
    if(!*l)
      *l = (char **)fs_get((count + 2) * sizeof(char *));
    else
      fs_resize((void **)l, (count + 2) * sizeof(char *));

    (*l)[count]     = cpystr(newval);
    (*l)[count + 1] = NULL;
}


int
reset_character_set_stuff(char **err)
{
    int use_system = 0;
    char buf[1000];

    if(err)
      *err = NULL;

    if(ps_global->display_charmap)
      fs_give((void **) &ps_global->display_charmap);

    if(ps_global->keyboard_charmap)
      fs_give((void **) &ps_global->keyboard_charmap);

    if(ps_global->posting_charmap)
      fs_give((void **) &ps_global->posting_charmap);

    if(ps_global->VAR_CHAR_SET)
      ps_global->display_charmap = cpystr(ps_global->VAR_CHAR_SET);
    else{
#if HAVE_LANGINFO_H && defined(CODESET)
      ps_global->display_charmap = cpystr(nl_langinfo(CODESET));
#else
      ps_global->display_charmap = cpystr("UTF-8");
#endif
    }

    if(!ps_global->display_charmap)
      ps_global->display_charmap = cpystr("US-ASCII");

    if(ps_global->VAR_KEY_CHAR_SET)
      ps_global->keyboard_charmap = cpystr(ps_global->VAR_KEY_CHAR_SET);
    else
      ps_global->keyboard_charmap = cpystr(ps_global->display_charmap);

    if(!ps_global->keyboard_charmap)
      ps_global->keyboard_charmap = cpystr("US-ASCII");

    if(F_ON(F_USE_SYSTEM_TRANS, ps_global)){
#if	PREREQ_FOR_SYS_TRANSLATION
	use_system++;
	/* This modifies its arguments */
	if(setup_for_input_output(use_system, &ps_global->display_charmap,
				  &ps_global->keyboard_charmap,
				  &ps_global->input_cs, (err && *err) ? NULL : err) == -1)
	  return -1;
#elif	_WINDOWS	
	if(err && !*err)
	  *err = cpystr(_("Option Use-System-Translation ignored due to missing system functionality"));
#endif
    }

    if(!use_system){
	if(setup_for_input_output(use_system, &ps_global->display_charmap,
				  &ps_global->keyboard_charmap,
				  &ps_global->input_cs, (err && *err) ? NULL : err) == -1)
	  return -1;
    }

    if(!use_system && ps_global->VAR_POST_CHAR_SET){
	ps_global->posting_charmap = cpystr(ps_global->VAR_POST_CHAR_SET);
	if(!posting_charset_is_supported(ps_global->posting_charmap)){
	    if(err && !*err){
		snprintf(buf, sizeof(buf),
			 _("Posting-Character-Set \"%s\" is unsupported, using UTF-8"),
			 ps_global->posting_charmap);
		*err = cpystr(buf);
	    }

	    fs_give((void **) &ps_global->posting_charmap);
	    ps_global->posting_charmap = cpystr("UTF-8");
	}
    }
    else{
	if(use_system && ps_global->VAR_POST_CHAR_SET
	   && strucmp(ps_global->VAR_POST_CHAR_SET, "UTF-8"))
	    if(err && !*err)
	      *err = cpystr(_("Posting-Character-Set is ignored with Use-System-Translation turned on"));

	ps_global->posting_charmap = cpystr("UTF-8");
    }

    set_locale_charmap(ps_global->keyboard_charmap);

    return(0);
}


/*
 * Given a single printer string from the config file, returns pointers
 * to alloc'd strings containing the printer nickname, the command,
 * the init string, the trailer string, everything but the nickname string,
 * and everything but the command string.  All_but_cmd includes the trailing
 * space at the end (the one before the command) but all_but_nick does not
 * include the leading space (the one before the [).
 * If you pass in a pointer it is guaranteed to come back pointing to an
 * allocated string, even if it is just an empty string.  It is ok to pass
 * NULL for any of the six return strings.
 */
void
parse_printer(char *input, char **nick, char **cmd, char **init, char **trailer,
	      char **all_but_nick, char **all_but_cmd)
{
    char *p, *q, *start, *saved_options = NULL;
    int tmpsave, cnt;

    if(!input)
      input = "";

    if(nick || all_but_nick){
	if(p = srchstr(input, " [")){
	    if(all_but_nick)
	      *all_but_nick = cpystr(p+1);

	    if(nick){
		while(p-1 > input && isspace((unsigned char)*(p-1)))
		  p--;

		tmpsave = *p;
		*p = '\0';
		*nick = cpystr(input);
		*p = tmpsave;
	    }
	}
	else{
	    if(nick)
	      *nick = cpystr("");

	    if(all_but_nick)
	      *all_but_nick = cpystr(input);
	}
    }

    if(p = srchstr(input, "] ")){
	do{
	    ++p;
	}while(isspace((unsigned char)*p));

	tmpsave = *p;
	*p = '\0';
	saved_options = cpystr(input);
	*p = tmpsave;
    }
    else
      p = input;
    
    if(cmd)
      *cmd = cpystr(p);

    if(init){
	if(saved_options && (p = srchstr(saved_options, "INIT="))){
	    start = p + strlen("INIT=");
	    for(cnt=0, p = start; *p && *(p+1) && isxpair(p); p += 2)
	      cnt++;
	    
	    q = *init = (char *)fs_get((cnt + 1) * sizeof(char));
	    for(p = start; *p && *(p+1) && isxpair(p); p += 2)
	      *q++ = read_hex(p);
	    
	    *q = '\0';
	}
	else
	  *init = cpystr("");
    }

    if(trailer){
	if(saved_options && (p = srchstr(saved_options, "TRAILER="))){
	    start = p + strlen("TRAILER=");
	    for(cnt=0, p = start; *p && *(p+1) && isxpair(p); p += 2)
	      cnt++;
	    
	    q = *trailer = (char *)fs_get((cnt + 1) * sizeof(char));
	    for(p = start; *p && *(p+1) && isxpair(p); p += 2)
	      *q++ = read_hex(p);
	    
	    *q = '\0';
	}
	else
	  *trailer = cpystr("");
    }

    if(all_but_cmd){
	if(saved_options)
	  *all_but_cmd = saved_options;
	else
	  *all_but_cmd = cpystr("");
    }
    else if(saved_options)
      fs_give((void **)&saved_options);
}


int
copy_pinerc(char *local, char *remote, char **err_msg)
{
    return(copy_localfile_to_remotefldr(RemImap, local, remote,
					(void *)REMOTE_PINERC_SUBTYPE,
					err_msg));
}


int
copy_abook(char *local, char *remote, char **err_msg)
{
    return(copy_localfile_to_remotefldr(RemImap, local, remote,
					(void *)REMOTE_ABOOK_SUBTYPE,
					err_msg));
}


/*
 * Copy local file to remote folder.
 *
 * Args remotetype -- type of remote folder
 *           local -- name of local file
 *          remote -- name of remote folder
 *         subtype --
 *
 * Returns 0 on success.
 */
int
copy_localfile_to_remotefldr(RemType remotetype, char *local, char *remote,
			     void *subtype, char **err_msg)
{
    int        retfail = -1;
    unsigned   flags;
    REMDATA_S *rd;

    dprint((9, "copy_localfile_to_remotefldr(%s,%s)\n",
	       local ? local : "<null>",
	       remote ? remote : "<null>"));

    *err_msg = (char *)fs_get(MAXPATH * sizeof(char));

    if(!local || !*local){
	snprintf(*err_msg, MAXPATH, _("No local file specified"));
	return(retfail);
    }

    if(!remote || !*remote){
	snprintf(*err_msg, MAXPATH, _("No remote folder specified"));
	return(retfail);
    }

    if(!IS_REMOTE(remote)){
	snprintf(*err_msg, MAXPATH, _("Remote folder name \"%s\" %s"), remote,
		(*remote != '{') ? _("must begin with \"{\"") : _("not valid"));
	return(retfail);
    }

    if(IS_REMOTE(local)){
	snprintf(*err_msg, MAXPATH, _("First argument \"%s\" must be a local filename"),
	        local);
	return(retfail);
    }

    if(can_access(local, ACCESS_EXISTS) != 0){
	snprintf(*err_msg, MAXPATH, _("Local file \"%s\" does not exist"), local);
	return(retfail);
    }

    if(can_access(local, READ_ACCESS) != 0){
	snprintf(*err_msg, MAXPATH, _("Can't read local file \"%s\": %s"),
	        local, error_description(errno));
	return(retfail);
    }

    /*
     * Check if remote folder exists and create it if it doesn't.
     */
    flags = 0;
    rd = rd_create_remote(remotetype, remote, subtype,
			  &flags, _("Error: "), _("Can't copy to remote folder."));
    
    if(!rd || rd->access == NoExists){
	snprintf(*err_msg, MAXPATH, _("Can't create \"%s\""), remote);
	if(rd)
	  rd_free_remdata(&rd);

	return(retfail);
    }

    if(rd->access == MaybeRorW)
      rd->access = ReadWrite;

    rd->flags |= (NO_META_UPDATE | DO_REMTRIM);
    rd->lf = cpystr(local);

    rd_open_remote(rd);
    if(!rd_stream_exists(rd)){
	snprintf(*err_msg, MAXPATH, _("Can't open remote folder \"%s\""), rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    if(rd_remote_is_readonly(rd)){
	snprintf(*err_msg, MAXPATH, _("Remote folder \"%s\" is readonly"), rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    switch(rd->type){
      case RemImap:
	/*
	 * Empty folder, add a header msg.
	 */
	if(rd->t.i.stream->nmsgs == 0){
	    if(rd_init_remote(rd, 1) != 0){
		snprintf(*err_msg, MAXPATH,
		  _("Failed initializing remote folder \"%s\", check debug file"),
		  rd->rn);
		rd_free_remdata(&rd);
		return(retfail);
	    }
	}

	fs_give((void **)err_msg);
	*err_msg = NULL;
	if(rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, err_msg)){
		rd_free_remdata(&rd);
		return(retfail);
	}

	break;

      default:
	break;
    }

    if(rd_update_remote(rd, NULL) != 0){
	snprintf(*err_msg, MAXPATH, _("Error copying to remote folder \"%s\""), rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    rd_update_metadata(rd, NULL);
    rd_close_remdata(&rd);
    
    fs_give((void **)err_msg);
    return(0);
}


/*----------------------------------------------------------------------
    Panic pine - call on detected programmatic errors to exit pine, with arg

  Input: message --  printf styule string for panic message (see above)
         arg     --  argument for printf string

 Result: The various tty modes are restored
         If debugging is active a core dump will be generated
         Exits Pine
  ----*/
void
panic1(char *message, char *arg)
{
    char buf1[1001], buf2[1001];

    snprintf(buf1, sizeof(buf1), "%.*s", MAX(sizeof(buf1) - 1 - strlen(message), 0), arg);
    snprintf(buf2, sizeof(buf2), message, buf1);
    panic(buf2);
}


/*
 *
 */
HelpType
config_help(int var, int feature)
{
    switch(var){
      case V_FEATURE_LIST :
	return(feature_list_help(feature));
	break;

      case V_PERSONAL_NAME :
	return(h_config_pers_name);
      case V_USER_ID :
	return(h_config_user_id);
      case V_USER_DOMAIN :
	return(h_config_user_dom);
      case V_SMTP_SERVER :
	return(h_config_smtp_server);
      case V_NNTP_SERVER :
	return(h_config_nntp_server);
      case V_INBOX_PATH :
	return(h_config_inbox_path);
      case V_PRUNED_FOLDERS :
	return(h_config_pruned_folders);
      case V_DEFAULT_FCC :
	return(h_config_default_fcc);
      case V_DEFAULT_SAVE_FOLDER :
	return(h_config_def_save_folder);
      case V_POSTPONED_FOLDER :
	return(h_config_postponed_folder);
      case V_READ_MESSAGE_FOLDER :
	return(h_config_read_message_folder);
      case V_FORM_FOLDER :
	return(h_config_form_folder);
      case V_ARCHIVED_FOLDERS :
	return(h_config_archived_folders);
      case V_SIGNATURE_FILE :
	return(h_config_signature_file);
      case V_LITERAL_SIG :
	return(h_config_literal_sig);
      case V_INIT_CMD_LIST :
	return(h_config_init_cmd_list);
      case V_COMP_HDRS :
	return(h_config_comp_hdrs);
      case V_CUSTOM_HDRS :
	return(h_config_custom_hdrs);
      case V_VIEW_HEADERS :
	return(h_config_viewer_headers);
      case V_VIEW_MARGIN_LEFT :
	return(h_config_viewer_margin_left);
      case V_VIEW_MARGIN_RIGHT :
	return(h_config_viewer_margin_right);
      case V_QUOTE_SUPPRESSION :
	return(h_config_quote_suppression);
      case V_SAVED_MSG_NAME_RULE :
	return(h_config_saved_msg_name_rule);
      case V_FCC_RULE :
	return(h_config_fcc_rule);
      case V_SORT_KEY :
	return(h_config_sort_key);
      case V_AB_SORT_RULE :
	return(h_config_ab_sort_rule);
      case V_FLD_SORT_RULE :
	return(h_config_fld_sort_rule);
      case V_POST_CHAR_SET :
	return(h_config_post_char_set);
      case V_KEY_CHAR_SET :
	return(h_config_key_char_set);
      case V_CHAR_SET :
	return(h_config_char_set);
      case V_EDITOR :
	return(h_config_editor);
      case V_SPELLER :
	return(h_config_speller);
      case V_DISPLAY_FILTERS :
	return(h_config_display_filters);
      case V_SEND_FILTER :
	return(h_config_sending_filter);
      case V_ALT_ADDRS :
	return(h_config_alt_addresses);
      case V_KEYWORDS :
	return(h_config_keywords);
      case V_KW_BRACES :
	return(h_config_kw_braces);
      case V_KW_COLORS :
	return(h_config_kw_color);
      case V_ABOOK_FORMATS :
	return(h_config_abook_formats);
      case V_INDEX_FORMAT :
	return(h_config_index_format);
      case V_INCCHECKTIMEO :
	return(h_config_incoming_timeo);
      case V_INCCHECKINTERVAL :
	return(h_config_incoming_interv);
      case V_INCCHECKLIST :
	return(h_config_incoming_list);
      case V_OVERLAP :
	return(h_config_viewer_overlap);
      case V_MAXREMSTREAM :
	return(h_config_maxremstream);
      case V_PERMLOCKED :
	return(h_config_permlocked);
      case V_MARGIN :
	return(h_config_scroll_margin);
      case V_DEADLETS :
	return(h_config_deadlets);
      case V_FILLCOL :
	return(h_config_composer_wrap_column);
      case V_TCPOPENTIMEO :
	return(h_config_tcp_open_timeo);
      case V_TCPREADWARNTIMEO :
	return(h_config_tcp_readwarn_timeo);
      case V_TCPWRITEWARNTIMEO :
	return(h_config_tcp_writewarn_timeo);
      case V_TCPQUERYTIMEO :
	return(h_config_tcp_query_timeo);
      case V_RSHOPENTIMEO :
	return(h_config_rsh_open_timeo);
      case V_SSHOPENTIMEO :
	return(h_config_ssh_open_timeo);
      case V_USERINPUTTIMEO :
	return(h_config_user_input_timeo);
      case V_REMOTE_ABOOK_VALIDITY :
	return(h_config_remote_abook_validity);
      case V_REMOTE_ABOOK_HISTORY :
	return(h_config_remote_abook_history);
      case V_INCOMING_FOLDERS :
	return(h_config_incoming_folders);
      case V_FOLDER_SPEC :
	return(h_config_folder_spec);
      case V_NEWS_SPEC :
	return(h_config_news_spec);
      case V_ADDRESSBOOK :
	return(h_config_address_book);
      case V_GLOB_ADDRBOOK :
	return(h_config_glob_addrbook);
      case V_LAST_VERS_USED :
	return(h_config_last_vers);
      case V_SENDMAIL_PATH :
	return(h_config_sendmail_path);
      case V_OPER_DIR :
	return(h_config_oper_dir);
      case V_RSHPATH :
	return(h_config_rshpath);
      case V_RSHCMD :
	return(h_config_rshcmd);
      case V_SSHPATH :
	return(h_config_sshpath);
      case V_SSHCMD :
	return(h_config_sshcmd);
      case V_NEW_VER_QUELL :
	return(h_config_new_ver_quell);
      case V_DISABLE_DRIVERS :
	return(h_config_disable_drivers);
      case V_DISABLE_AUTHS :
	return(h_config_disable_auths);
      case V_REMOTE_ABOOK_METADATA :
	return(h_config_abook_metafile);
      case V_REPLY_STRING :
	return(h_config_reply_indent_string);
      case V_WORDSEPS :
	return(h_config_wordseps);
      case V_QUOTE_REPLACE_STRING :
	return(h_config_quote_replace_string);
      case V_REPLY_INTRO :
	return(h_config_reply_intro);
      case V_EMPTY_HDR_MSG :
	return(h_config_empty_hdr_msg);
      case V_STATUS_MSG_DELAY :
	return(h_config_status_msg_delay);
      case V_ACTIVE_MSG_INTERVAL :
	return(h_config_active_msg_interval);
      case V_MAILCHECK :
	return(h_config_mailcheck);
      case V_MAILCHECKNONCURR :
	return(h_config_mailchecknoncurr);
      case V_MAILDROPCHECK :
	return(h_config_maildropcheck);
      case V_NNTPRANGE :
	return(h_config_nntprange);
      case V_NEWS_ACTIVE_PATH :
	return(h_config_news_active);
      case V_NEWS_SPOOL_DIR :
	return(h_config_news_spool);
      case V_IMAGE_VIEWER :
	return(h_config_image_viewer);
      case V_USE_ONLY_DOMAIN_NAME :
	return(h_config_domain_name);
      case V_LAST_TIME_PRUNE_QUESTION :
	return(h_config_prune_date);
      case V_UPLOAD_CMD:
	return(h_config_upload_cmd);
      case V_UPLOAD_CMD_PREFIX:
	return(h_config_upload_prefix);
      case V_DOWNLOAD_CMD:
	return(h_config_download_cmd);
      case V_DOWNLOAD_CMD_PREFIX:
	return(h_config_download_prefix);
      case V_GOTO_DEFAULT_RULE:
	return(h_config_goto_default);
      case V_INCOMING_STARTUP:
	return(h_config_inc_startup);
      case V_PRUNING_RULE:
	return(h_config_pruning_rule);
      case V_REOPEN_RULE:
	return(h_config_reopen_rule);
      case V_THREAD_DISP_STYLE:
	return(h_config_thread_disp_style);
      case V_THREAD_INDEX_STYLE:
	return(h_config_thread_index_style);
      case V_THREAD_MORE_CHAR:
	return(h_config_thread_indicator_char);
      case V_THREAD_EXP_CHAR:
	return(h_config_thread_exp_char);
      case V_THREAD_LASTREPLY_CHAR:
	return(h_config_thread_lastreply_char);
      case V_MAILCAP_PATH :
	return(h_config_mailcap_path);
      case V_MIMETYPE_PATH :
	return(h_config_mimetype_path);
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
      case V_FIFOPATH :
	return(h_config_fifopath);
#endif
      case V_NMW_WIDTH :
	return(h_config_newmailwidth);
      case V_NEWSRC_PATH :
	return(h_config_newsrc_path);
      case V_BROWSER :
	return(h_config_browser);
#if defined(DOS) || defined(OS2)
      case V_FILE_DIR :
	return(h_config_file_dir);
#endif
      case V_NORM_FORE_COLOR :
      case V_NORM_BACK_COLOR :
	return(h_config_normal_color);
      case V_REV_FORE_COLOR :
      case V_REV_BACK_COLOR :
	return(h_config_reverse_color);
      case V_TITLE_FORE_COLOR :
      case V_TITLE_BACK_COLOR :
	return(h_config_title_color);
      case V_TITLECLOSED_FORE_COLOR :
      case V_TITLECLOSED_BACK_COLOR :
	return(h_config_titleclosed_color);
      case V_STATUS_FORE_COLOR :
      case V_STATUS_BACK_COLOR :
	return(h_config_status_color);
      case V_SLCTBL_FORE_COLOR :
      case V_SLCTBL_BACK_COLOR :
	return(h_config_slctbl_color);
      case V_QUOTE1_FORE_COLOR :
      case V_QUOTE2_FORE_COLOR :
      case V_QUOTE3_FORE_COLOR :
      case V_QUOTE1_BACK_COLOR :
      case V_QUOTE2_BACK_COLOR :
      case V_QUOTE3_BACK_COLOR :
	return(h_config_quote_color);
      case V_SIGNATURE_FORE_COLOR :
      case V_SIGNATURE_BACK_COLOR :
	return(h_config_signature_color);
      case V_PROMPT_FORE_COLOR :
      case V_PROMPT_BACK_COLOR :
	return(h_config_prompt_color);
      case V_IND_PLUS_FORE_COLOR :
      case V_IND_IMP_FORE_COLOR :
      case V_IND_DEL_FORE_COLOR :
      case V_IND_ANS_FORE_COLOR :
      case V_IND_NEW_FORE_COLOR :
      case V_IND_UNS_FORE_COLOR :
      case V_IND_REC_FORE_COLOR :
      case V_IND_PLUS_BACK_COLOR :
      case V_IND_IMP_BACK_COLOR :
      case V_IND_DEL_BACK_COLOR :
      case V_IND_ANS_BACK_COLOR :
      case V_IND_NEW_BACK_COLOR :
      case V_IND_UNS_BACK_COLOR :
      case V_IND_REC_BACK_COLOR :
	return(h_config_index_color);
      case V_IND_OP_FORE_COLOR :
      case V_IND_OP_BACK_COLOR :
	return(h_config_index_opening_color);
      case V_IND_ARR_FORE_COLOR :
      case V_IND_ARR_BACK_COLOR :
	return(h_config_index_arrow_color);
      case V_KEYLABEL_FORE_COLOR :
      case V_KEYLABEL_BACK_COLOR :
	return(h_config_keylabel_color);
      case V_KEYNAME_FORE_COLOR :
      case V_KEYNAME_BACK_COLOR :
	return(h_config_keyname_color);
      case V_METAMSG_FORE_COLOR :
      case V_METAMSG_BACK_COLOR :
	return(h_config_metamsg_color);
      case V_VIEW_HDR_COLORS :
	return(h_config_customhdr_color);
      case V_PRINTER :
	return(h_config_printer);
      case V_PERSONAL_PRINT_CATEGORY :
	return(h_config_print_cat);
      case V_PERSONAL_PRINT_COMMAND :
	return(h_config_print_command);
      case V_PAT_ROLES :
	return(h_config_pat_roles);
      case V_PAT_FILTS :
	return(h_config_pat_filts);
      case V_PAT_SCORES :
	return(h_config_pat_scores);
      case V_PAT_INCOLS :
	return(h_config_pat_incols);
      case V_PAT_OTHER :
	return(h_config_pat_other);
      case V_INDEX_COLOR_STYLE :
	return(h_config_index_color_style);
      case V_TITLEBAR_COLOR_STYLE :
	return(h_config_titlebar_color_style);
#ifdef	_WINDOWS
      case V_FONT_NAME :
	return(h_config_font_name);
      case V_FONT_SIZE :
	return(h_config_font_size);
      case V_FONT_STYLE :
	return(h_config_font_style);
      case V_FONT_CHAR_SET :
	return(h_config_font_char_set);
      case V_PRINT_FONT_NAME :
	return(h_config_print_font_name);
      case V_PRINT_FONT_SIZE :
	return(h_config_print_font_size);
      case V_PRINT_FONT_STYLE :
	return(h_config_print_font_style);
      case V_PRINT_FONT_CHAR_SET :
	return(h_config_print_font_char_set);
      case V_WINDOW_POSITION :
	return(h_config_window_position);
      case V_CURSOR_STYLE :
	return(h_config_cursor_style);
#else
      case V_COLOR_STYLE :
	return(h_config_color_style);
#endif
#ifdef	ENABLE_LDAP
      case V_LDAP_SERVERS :
	return(h_config_ldap_servers);
#endif
      case V_WP_COLUMNS :
	return(h_config_wp_columns);
      default :
	return(NO_HELP);
    }
}


char **
get_supported_options(void)
{
    char         **config;
    DRIVER        *d;
    AUTHENTICATOR *a;
    char          *title = _("Supported features in this Alpine");
    char           sbuf[MAX_SCREEN_COLS+1];
    int            cnt, alcnt, len, cols, disabled, any_disabled = 0;;

    /*
     * Line count:
     *   Title + blank			= 2
     *   SSL Title + SSL line + blank	= 3
     *   Auth title + blank		= 2
     *   Driver title + blank		= 2
     *   LDAP title + LDAP line 	= 2
     *   Disabled explanation + blank line = 4
     *   end				= 1
     */
    cnt = 16;
    for(a = mail_lookup_auth(1); a; a = a->next)
      cnt++;
    for(d = (DRIVER *)mail_parameters(NIL, GET_DRIVERS, NIL);
	d; d = d->next)
      cnt++;

    alcnt = cnt;
    config = (char **) fs_get(alcnt * sizeof(char *));
    memset(config, 0, alcnt * sizeof(char *));

    cols = ps_global->ttyo ? ps_global->ttyo->screen_cols : 0;
    len = utf8_width(title);
    snprintf(sbuf, sizeof(sbuf), "%*s%s", cols > len ? (cols-len)/2 : 0, "", title);

    cnt = 0;
    if(cnt < alcnt)
      config[cnt] = cpystr(sbuf);

    if(++cnt < alcnt)
      config[cnt] = cpystr("");

    if(++cnt < alcnt)
      /* TRANSLATORS: headings */
      config[cnt] = cpystr(_("Encryption:"));

    if(++cnt < alcnt && mail_parameters(NIL, GET_SSLDRIVER, NIL))
      config[cnt] = cpystr(_("  TLS and SSL"));
    else
      config[cnt] = cpystr(_("  None (no TLS or SSL)"));

    if(++cnt < alcnt)
      config[cnt] = cpystr("");

    if(++cnt < alcnt)
      config[cnt] = cpystr(_("Authenticators:"));

    for(a = mail_lookup_auth(1); a; a = a->next){
	disabled = (a->client == NULL && a->server == NULL);
	any_disabled += disabled;
	snprintf(sbuf, sizeof(sbuf), "  %s%s", a->name, disabled ? " (disabled)" : "");
	if(++cnt < alcnt)
	  config[cnt] = cpystr(sbuf);
    }

    if(++cnt < alcnt)
      config[cnt] = cpystr("");

    if(++cnt < alcnt)
      config[cnt] = cpystr(_("Mailbox drivers:"));

    for(d = (DRIVER *)mail_parameters(NIL, GET_DRIVERS, NIL);
	d; d = d->next){
	disabled = (d->flags & DR_DISABLE);
	any_disabled += disabled;
	snprintf(sbuf, sizeof(sbuf), "  %s%s", d->name, disabled ? " (disabled)" : "");
	if(++cnt < alcnt)
	  config[cnt] = cpystr(sbuf);
    }

    if(++cnt < alcnt)
      config[cnt] = cpystr("");

    if(++cnt < alcnt)
      config[cnt] = cpystr(_("Directories:"));

#ifdef	ENABLE_LDAP
    if(++cnt < alcnt)
      config[cnt] = cpystr("  LDAP");
#else
    if(++cnt < alcnt)
      config[cnt] = cpystr("  None (no LDAP)");
#endif

    if(any_disabled){
	if(++cnt < alcnt)
	  config[cnt] = cpystr("");

	if(ps_global->ttyo){
	  if(++cnt < alcnt)
	    config[cnt] = cpystr(_("Authenticators may be disabled because of the \"disable-these-authenticators\" hidden config option. Mailbox drivers may be disabled because of the \"disable-these-drivers\" hidden config option."));
	}
	else{
	    if(++cnt < alcnt)
	      config[cnt] = cpystr(_("Authenticators may be disabled because of the \"disable-these-authenticators\""));
	    if(++cnt < alcnt)
	      config[cnt] = cpystr(_("hidden config option. Mailbox drivers may be disabled because of the"));
	    if(++cnt < alcnt)
	      config[cnt] = cpystr(_("\"disable-these-drivers\" hidden config option."));
	}
    }

    if(++cnt < alcnt)
      config[cnt] = NULL;

    return(config);
}


void
dump_supported_options(void)
{
    char **config;
    char **p;
    FILE  *f = stdout;

    config = get_supported_options();
    if(config){
	display_args_err(NULL, config, 0);
	free_list_array(&config);
    }
}


unsigned
reset_startup_rule(MAILSTREAM *stream)
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    unsigned      startup_rule;

    startup_rule = IS_NOTSET;

    if(stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, stream, NULL, NULL, NULL,
			     SE_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus)
	  startup_rule = pat->action->startup_rule;
    }

    return(startup_rule);
}


#ifdef	_WINDOWS

char *
transformed_color(old)
    char *old;
{
    if(!old)
      return("");

    if(!struncmp(old, "color008", 8))
      return("colorlgr");
    else if(!struncmp(old, "color009", 8))
      return("colormgr");
    else if(!struncmp(old, "color010", 8))
      return("colordgr");

    return("");
}


/*
 * If this is the first time we've run a version > 4.40, and there
 * is evidence that the config file has not been used by unix pine,
 * then we convert color008 to colorlgr, color009 to colormgr, and
 * color010 to colordgr. If the config file is being used by
 * unix pine then color008 may really supposed to be color008, color009
 * may really supposed to be red, and color010 may really supposed to be
 * green. Same if we've already run 4.41 or higher previously.
 *
 * Returns 0 if no changes, > 0 if something was changed.
 */
int
convert_pc_gray_names(ps, prc, which)
    struct pine *ps;
    PINERC_S    *prc;
    EditWhich    which;
{
    struct variable *v;
    int              ret = 0, ic = 0;
    char           **s, *t, *p, *pstr, *new, *pval, **apval, **lval;

    for(v = ps->vars; v->name; v++){
	if(!color_holding_var(ps, v) || v == &ps->vars[V_KW_COLORS])
	  continue;
	
	if(v == &ps->vars[V_VIEW_HDR_COLORS]){

	    if((lval = LVAL(v,which)) != NULL){
		/* fix these in place */
		for(s = lval; (t = *s) != NULL; s++){
		    if((p = srchstr(t, "FG=color008")) ||
		       (p = srchstr(t, "FG=color009")) ||
		       (p = srchstr(t, "FG=color010"))){
			strncpy(p+3, transformed_color(p+3), 8);
			ret++;
		    }

		    if((p = srchstr(t, "BG=color008")) ||
		       (p = srchstr(t, "BG=color009")) ||
		       (p = srchstr(t, "BG=color010"))){
			strncpy(p+3, transformed_color(p+3), 8);
			ret++;
		    }
		}
	    }
	}
	else{
	    if((pval = PVAL(v,which)) != NULL){
		apval = APVAL(v,which);
		if(apval && (!strucmp(pval, "color008") ||
		             !strucmp(pval, "color009") ||
		             !strucmp(pval, "color010"))){
		    new = transformed_color(pval);
		    if(*apval)
		      fs_give((void **)apval);

		    *apval = cpystr(new);
		    ret++;
		}
	    }
	}
    }

    v = &ps->vars[V_PAT_INCOLS];
    if((lval = LVAL(v,which)) != NULL){
	for(s = lval; (t = *s) != NULL; s++){
	    if((pstr = srchstr(t, "action=")) != NULL){
		if((p = srchstr(pstr, "FG=color008")) ||
		   (p = srchstr(pstr, "FG=color009")) ||
		   (p = srchstr(pstr, "FG=color010"))){
		    strncpy(p+3, transformed_color(p+3), 8);
		    ic++;
		}

		if((p = srchstr(pstr, "BG=color008")) ||
		   (p = srchstr(pstr, "BG=color009")) ||
		   (p = srchstr(pstr, "BG=color010"))){
		    strncpy(p+3, transformed_color(p+3), 8);
		    ic++;
		}
	    }
	}
    }

    if(ic)
      set_current_val(&ps->vars[V_PAT_INCOLS], TRUE, TRUE);

    return(ret+ic);
}


int
unix_color_style_in_pinerc(prc)
    PINERC_S *prc;
{
    PINERC_LINE *pline;

    for(pline = prc ? prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->line && !struncmp("color-style=", pline->line, 12))
	return(1);
    
    return(0);
}

char *
pcpine_general_help(titlebuf)
    char *titlebuf;
{
    if(titlebuf)
      strcpy(titlebuf, "PC Alpine For Windows");

    return(pcpine_help(h_pine_for_windows));
}

#endif	/* _WINDOWS */