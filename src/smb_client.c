#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "smb_client.h"

struct SmbSession {
	struct smb2_context *smb2;
};

SmbSession *smb_connect(const Server *server, int timeout_seconds, SmbError *out_error)
{
	struct smb2_context *smb2 = smb2_init_context();
	if (!smb2) {
		*out_error = SMB_ERR_FAILED;
		return NULL;
	}

	smb2_set_timeout(smb2, timeout_seconds);
	smb2_set_authentication(smb2, SMB2_SEC_NTLMSSP);
	smb2_set_password(smb2, server->password);
	smb2_set_domain(smb2, server->domain);

	char host[SERVER_STR_MAX + 16];
	snprintf(host, sizeof(host), "%s:%d", server->host, server->port);

	const char *user = server->username[0] ? server->username : NULL;

	if (smb2_connect_share(smb2, host, server->share, user) != 0) {
		*out_error = SMB_ERR_FAILED;
		smb2_destroy_context(smb2);
		return NULL;
	}

	SmbSession *session = malloc(sizeof(SmbSession));
	session->smb2 = smb2;
	*out_error = SMB_OK;
	return session;
}

void smb_disconnect(SmbSession *session)
{
	if (!session) return;
	smb2_disconnect_share(session->smb2);
	smb2_destroy_context(session->smb2);
	free(session);
}

int smb_list(SmbSession *session, const char *remote_path, BrowseEntry *out, int max_entries, SmbError *out_error)
{
	struct smb2dir *dir = smb2_opendir(session->smb2, remote_path);
	if (!dir) {
		*out_error = SMB_ERR_FAILED;
		return -1;
	}

	int count = 0;
	struct smb2dirent *entry;
	while (count < max_entries && (entry = smb2_readdir(session->smb2, dir))) {
		if (entry->st.smb2_type != SMB2_TYPE_DIRECTORY) continue;
		if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) continue;

		strncpy(out[count].name, entry->name, BROWSE_STR_MAX - 1);
		out[count].name[BROWSE_STR_MAX - 1] = '\0';
		out[count].is_dir = true;
		count++;
	}

	smb2_closedir(session->smb2, dir);
	*out_error = SMB_OK;
	return count;
}
