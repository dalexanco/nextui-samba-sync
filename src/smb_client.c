#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
