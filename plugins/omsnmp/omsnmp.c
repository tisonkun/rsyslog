/* omsnmp.c
 *
 * This module sends an snmp trap. More text will come here soon ^^
 *
 * This module will become part of the CVS and the rsyslog project because I think
 * it is a generally useful debugging, testing and development aid for everyone
 * involved with rsyslog.
 *
 * CURRENT SUPPORTED COMMANDS:
 *
 * :omsnmp:sleep <seconds> <milliseconds>
 *
 * Must be specified exactly as above. Keep in mind milliseconds are a millionth
 * of a second!
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <assert.h>
#include "syslogd.h"
#include "syslogd-types.h"
#include "cfsysline.h"
#include "module-template.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "omsnmp.h"

MODULE_TYPE_OUTPUT

/* internal structures
 */
DEF_OMOD_STATIC_DATA

/* Default static snmp OID's */
static oid             objid_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
/*static oid             objid_sysdescr[] = { 1, 3, 6, 1, 2, 1, 1, 1, 0 };
static oid             objid_sysuptime[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
static oid             objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };*/

static uchar* pszTarget = NULL;
/* note using an unsigned for a port number is not a good idea from an IPv6 point of view */
static int iPort = 0;
static uchar* pszCommunity = NULL;
static uchar* pszEnterpriseOID = NULL;
static uchar* pszSyslogMessageOID = NULL;
static int iSpecificType = 0;
static int iTrapType = SNMP_TRAP_ENTERPRISESPECIFIC;/*Default is SNMP_TRAP_ENTERPRISESPECIFIC */
/* 
			Possible Values
	SNMP_TRAP_COLDSTART				(0)
	SNMP_TRAP_WARMSTART				(1)
	SNMP_TRAP_LINKDOWN				(2)
	SNMP_TRAP_LINKUP				(3)
	SNMP_TRAP_AUTHFAIL				(4)
	SNMP_TRAP_EGPNEIGHBORLOSS		(5)
	SNMP_TRAP_ENTERPRISESPECIFIC	(6)
*/

typedef struct _instanceData {
	uchar	szTarget[MAXHOSTNAMELEN+1];					/* IP/hostname of Snmp Target*/ 
	uchar	szTargetAndPort[MAXHOSTNAMELEN+1];			/* IP/hostname + Port,needed format for SNMP LIB */ 
	uchar	szCommunity[OMSNMP_MAXCOMMUNITYLENGHT+1];	/* Snmp Community */ 
	uchar	szEnterpriseOID[OMSNMP_MAXOIDLENGHT+1];		/* Snmp Enterprise OID - default is (1.3.6.1.4.1.3.1.1 = enterprises.cmu.1.1) */ 
	uchar	szSyslogMessageOID[OMSNMP_MAXOIDLENGHT+1];	/* Snmp OID used for the Syslog Message - default is 1.3.6.1.4.1 - .iso.org.dod.internet.private.enterprises */ 
	int iPort;											/* Target Port */
	int iTrapType;										/* Snmp TrapType or GenericType */
	int iSpecificType;									/* Snmp Specific Type */

} instanceData;

BEGINcreateInstance
CODESTARTcreateInstance
ENDcreateInstance


BEGINdbgPrintInstInfo
CODESTARTdbgPrintInstInfo
	dbgprintf("Target: %s\n", pData->szTarget);
	dbgprintf("Port: %d\n", pData->iPort);
	dbgprintf("Target+PortStr: %s\n", pData->szTargetAndPort);
	dbgprintf("Port: %d\n", pData->iPort);
	dbgprintf("Community: %s\n", pData->szCommunity);
	dbgprintf("EnterpriseOID: %s\n", pData->szEnterpriseOID);
	dbgprintf("SyslogMessageOID: %s\n", pData->szSyslogMessageOID);
	dbgprintf("TrapType: %d\n", pData->iTrapType);
	dbgprintf("SpecificType: %d\n", pData->iSpecificType);
ENDdbgPrintInstInfo


BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	/* we are not compatible with repeated msg reduction feature, so do not allow it */
ENDisCompatibleWithFeature


BEGINtryResume
CODESTARTtryResume
ENDtryResume


/* 
*	Helper function - not used yet!
*/
static int snmp_input_sender(int operation, netsnmp_session * session, int reqid, netsnmp_pdu *pdu, void *magic)
{
	//nothing todo yet!
    return 1;
}

/* 
*	Helper function for parsing and converting dns/ip addresses into in_addr_t structs
*/
static in_addr_t omsnmp_parse_address(char* address)
{
    in_addr_t       addr;
    struct sockaddr_in saddr;
    struct hostent *hp;

    if ((addr = inet_addr(address)) != -1)
        return addr;
    hp = gethostbyname(address);
    if (hp == NULL)
	{
		dbgprintf("parse_address failed\n");
        return NULL;
    } 
	else 
	{
        memcpy( &saddr.sin_addr, hp->h_addr_list[0], hp->h_length);
/*		memcpy( &server_addr.sin_addr.s_addr,hostInfo->h_addr_list[0], hostInfo->h_length);*/
        return saddr.sin_addr.s_addr;
    }
}

static rsRetVal omsnmp_sendsnmp(instanceData *pData, uchar *psz)
{
	DEFiRet;

	netsnmp_session session, *ss;
	netsnmp_pdu    *pdu = NULL;
	in_addr_t      *pdu_in_addr_t;
	oid             enterpriseoid[MAX_OID_LEN];
	size_t          enterpriseoidlen = MAX_OID_LEN;
	oid				oidSyslogMessage[MAX_OID_LEN];
	size_t			oLen = MAX_OID_LEN;
	int             status;
	char           *trap = NULL, *specific = NULL, *description = NULL, *agent = NULL;
	char		   *strErr = NULL;

//    char           *prognam;
	int             exitval = 0;

	assert(psz != NULL);

	dbgprintf( "omsnmp_sendsnmp: ENTER - Target = '%s' on Port = '%d' syslogmessage = '%s'\n", pData->szTarget, pData->iPort, (char*)psz);

	putenv(strdup("POSIXLY_CORRECT=1"));

	snmp_sess_init(&session);
	session.version = SNMP_VERSION_1;	/* HardCoded to SNMPv1 */
	session.callback = snmp_input_sender;
	session.callback_magic = NULL;
	session.peername = pData->szTargetAndPort;

	if (session.version == SNMP_VERSION_1 || session.version == SNMP_VERSION_2c)
	{	
		session.community = (unsigned char *) pData->szCommunity;
		session.community_len = strlen(pData->szCommunity);
	}

	ss = snmp_open(&session);
	if (ss == NULL) 
	{
		/*TODO diagnose snmp_open errors with the input netsnmp_session pointer */
		dbgprintf("omsnmp_sendsnmp: snmp_open to host '%s' on Port '%d' failed\n", pData->szTarget, pData->iPort);
        return RS_RET_FALSE;
	}

	if (session.version == SNMP_VERSION_1) 
	{
		pdu = snmp_pdu_create(SNMP_MSG_TRAP);
		pdu_in_addr_t = (in_addr_t *) pdu->agent_addr;

		/* Set enterprise */
		/*
		if (TRUE) /*(pConfig->getAstrEnterprise()->Length() <= 0)
		{
			/* Set default OID in this case 
			pdu->enterprise = (oid *) malloc(sizeof(objid_enterprise));
			memcpy(pdu->enterprise, objid_enterprise, sizeof(objid_enterprise));
			pdu->enterprise_length = sizeof(objid_enterprise) / sizeof(oid);
		}
		else
		{
*/

			/* TODO! */
			if (!snmp_parse_oid( pData->szEnterpriseOID, &enterpriseoid, &enterpriseoidlen ))
			{
				strErr = snmp_api_errstring(snmp_errno);
				dbgprintf("omsnmp_sendsnmp: Parsing EnterpriseOID failed '%s' with error '%s' \n", pData->szSyslogMessageOID, strErr);

				
				/* TODO! CLEANUP */
				return RS_RET_FALSE;
			}
			pdu->enterprise = (oid *) malloc(enterpriseoidlen * sizeof(oid));
			memcpy(pdu->enterprise, enterpriseoid, enterpriseoidlen * sizeof(oid));
			pdu->enterprise_length = enterpriseoidlen;
/*
		}
*/
		/* Set Source Agent */
		*pdu_in_addr_t = omsnmp_parse_address( (char*)pData->szTarget );

		/* Set Traptype TODO */
		pdu->trap_type = pData->iTrapType; 
		
		/* Set SpecificType TODO */
		pdu->specific_type = pData->iSpecificType;

		//--- Set Updtime
		pdu->time = get_uptime();
	}

	/* SET TRAP PARAMETER for SyslogMessage! */
/*	dbgprintf( "omsnmp_sendsnmp: SyslogMessage '%s'\n", psz );*/

	// First create new OID object
	if (snmp_parse_oid( pData->szSyslogMessageOID, &oidSyslogMessage, &oLen))
	{
		int iErrCode = snmp_add_var(pdu, &oidSyslogMessage, oLen, 's', psz);
		if (iErrCode)
		{
			const char *str = snmp_api_errstring(iErrCode);
			dbgprintf( "omsnmp_sendsnmp: Invalid SyslogMessage OID, error code '%d' - '%s'\n", iErrCode, str );

			/* TODO! CLEANUP */
			return RS_RET_FALSE;
		}
	}
	else
	{
		strErr = snmp_api_errstring(snmp_errno);
		dbgprintf("omsnmp_sendsnmp: Parsing SyslogMessageOID failed '%s' with error '%s' \n", pData->szSyslogMessageOID, strErr);

		/* TODO! CLEANUP */
		return RS_RET_FALSE;
	}

	
/*	iVarRet = snmp_add_var(pdu, (oid*) pMyWalker->m_myobjid, pMyWalker->m_myobjid_length, 
							pMyWalker->cType, astrTmpVal.getStringA() );
	if (iVarRet)
	{
		CHECKFORDEBUGLEVEL(DEBUGLEVEL_ERRORS) 
		{
			// --- Important debug output in this case
			wchar_t wsz[256];
			const char *str;
			str = snmp_api_errstring(iVarRet);
			_snwprintf(wsz, sizeof(wsz) / sizeof(wchar_t), L"ActionConfig SendSNMP|SendSNMP: Please check your configuration, Error adding SNMPVar '%s', SNMP Error '%S'\r\n", 
						pMyWalker->m_astrVarOID.getString(), str);
			DBMSG_ERRORS(wsz)
			// ---
		}
	}
*/

dbgprintf( "omsnmp_sendsnmp: before snmp_send\n");
	/* Send the TRAP */
	status = snmp_send(ss, pdu) == 0;
dbgprintf( "omsnmp_sendsnmp: after snmp_send\n");
	if (status)
	{
		int iErrorCode = 0;
		if (ss->s_snmp_errno != 0)
			iErrorCode = ss->s_snmp_errno;
		else
			iErrorCode = session.s_snmp_errno;
		
		/* Debug Output! */
		dbgprintf( "omsnmp_sendsnmp: snmp_send failed error '%d', Description='%s'\n", iErrorCode*(-1), api_errors[iErrorCode*(-1)]);

		/* Important we must free the PDU! */
		snmp_free_pdu(pdu);

		/* Close SNMP Session */
		snmp_close(ss);

		/* TODO! CLEANUP */
		return RS_RET_FALSE;
	}
	else
	{
		/* Just Close SNMP Session */
		snmp_close(ss);
	}

dbgprintf( "omsnmp_sendsnmp: LEAVE\n");

	RETiRet;
}


BEGINdoAction
CODESTARTdoAction
	/* Abort if the STRING is not set, should never happen */
	if (ppString[0] == NULL)
	{
		ABORT_FINALIZE(RS_RET_TRUE);
	}

/*	dbgprintf("omsnmp: Sending SNMP Trap to '%s' on Port '%d'\n", pData->szTarget, pData->iPort); */
	iRet = omsnmp_sendsnmp(pData, ppString[0]);
	if (iRet == RS_RET_FALSE)
	{
		/* TODO: CLEANUP! */
		ABORT_FINALIZE(RS_RET_SUSPENDED);
	}
finalize_it:
ENDdoAction

BEGINfreeInstance
CODESTARTfreeInstance
	/* we do not have instance data, so we do not need to
	 * do anything here. -- rgerhards, 2007-07-25
	 */
ENDfreeInstance


BEGINparseSelectorAct
CODESTARTparseSelectorAct
CODE_STD_STRING_REQUESTparseSelectorAct(1)
	/* code here is quick and dirty - if you like, clean it up. But keep
	 * in mind it is just a testing aid ;) -- rgerhards, 2007-12-31
	 */
	if(!strncmp((char*) p, ":omsnmp:", sizeof(":omsnmp:") - 1)) {
		p += sizeof(":omsnmp:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
	} else {
		ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
	}

	/* ok, if we reach this point, we have something for us */
	if((iRet = createInstance(&pData)) != RS_RET_OK)
		goto finalize_it;

	/* Failsave */
	if (pszTarget == NULL) {
		/* due to a problem in the framework, we can not return an error code
		 * right now, so we need to use a (useless) default.
		 */
		/* TODO: re-enable when rsyslogd supports it: ABORT_FINALIZE( RS_RET_PARAM_ERROR ); */
		strncpy( (char*) pData->szTarget, "127.0.0.1", sizeof("127.0.0.1") );
	} else {

	/* Copy Target */
	strncpy( (char*) pData->szTarget, (char*) pszTarget, strlen(pszTarget) );
	}

	/* Copy Community */
	if (pszCommunity == NULL)	/* Failsave */
		strncpy( (char*) pData->szCommunity, "public", sizeof("public") );
	else						/* Copy Target */
		strncpy( (char*) pData->szCommunity, (char*) pszCommunity, strlen(pszCommunity) );

	/* Copy Enterprise OID */
	if (pszEnterpriseOID == NULL)	/* Failsave */
		strncpy( (char*) pData->szEnterpriseOID, "1.3.6.1.4.1.3.1.1", sizeof("1.3.6.1.4.1.3.1.1") );
	else						/* Copy Target */
		strncpy( (char*) pData->szEnterpriseOID, (char*) pszEnterpriseOID, strlen(pszEnterpriseOID) );

	/* Copy SyslogMessage OID */
	if (pszSyslogMessageOID == NULL)	/* Failsave */
		strncpy( (char*) pData->szSyslogMessageOID, "1.3.6.1.6.3.1.1.4.3", sizeof("1.3.6.1.6.3.1.1.4.3") );
	else						/* Copy Target */
		strncpy( (char*) pData->szSyslogMessageOID, (char*) pszSyslogMessageOID, strlen(pszSyslogMessageOID) );

	/* Copy Port */
	if ( iPort == 0)		/* If no Port is set we use the default Port 162 */
		pData->iPort = 162;
	else
		pData->iPort = iPort;

	/* Copy SpecificType */
	if ( iSpecificType == 0)		/* If no iSpecificType is set, we use the default 0 */
		pData->iSpecificType = 0;
	else
		pData->iSpecificType = iSpecificType;

	/* Copy TrapType */
	if ( iTrapType < 0 && iTrapType >= 6)		/* Only allow values from 0 to 6 !*/
		pData->iTrapType = SNMP_TRAP_ENTERPRISESPECIFIC;
	else
		pData->iTrapType = iTrapType;

	/* Create string for session peername! */
	sprintf( pData->szTargetAndPort, "%s:%d", pszTarget, iPort );
	
	/* Print Debug info */
	dbgprintf("SNMPTarget: %s\n", pData->szTarget);
	dbgprintf("SNMPPort: %d\n", pData->iPort);
	dbgprintf("Target+PortStr: %s\n", pData->szTargetAndPort);
	dbgprintf("Community: %s\n", pData->szCommunity);
	dbgprintf("EnterpriseOID: %s\n", pData->szEnterpriseOID);
	dbgprintf("SyslogMessageOID: %s\n", pData->szSyslogMessageOID);
	dbgprintf("TrapType: %d\n", pData->iTrapType);
	dbgprintf("SpecificType: %d\n", pData->iSpecificType);

	/* process template */
	CHKiRet(cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar*) " StdFwdFmt"));

	/* Init NetSNMP library and read in MIB database */
	init_snmp("rsyslog");

	/* Set some defaults in the NetSNMP library */
	netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DEFAULT_PORT, pData->iPort );

CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINneedUDPSocket
CODESTARTneedUDPSocket
ENDneedUDPSocket

/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) *pp, void __attribute__((unused)) *pVal)
{
	if (pszTarget != NULL)
		free(pszTarget);
	pszTarget = NULL;
	iPort = 0;

	return RS_RET_OK;
}


BEGINmodExit
CODESTARTmodExit
if (pszTarget != NULL)
	free(pszTarget);	
ENDmodExit


BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_OMOD_QUERIES
ENDqueryEtryPt


BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = 1; /* so far, we only support the initial definition */
CODEmodInit_QueryRegCFSLineHdlr
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmptarget", 0, eCmdHdlrGetWord, NULL, &pszTarget, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmptargetport", 0, eCmdHdlrInt, NULL, &iPort, NULL));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpcommunity", 0, eCmdHdlrGetWord, NULL, &pszCommunity, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpenterpriseoid", 0, eCmdHdlrGetWord, NULL, &pszEnterpriseOID, STD_LOADABLE_MODULE_ID));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"actionsnmpsyslogmessageoid", 0, eCmdHdlrGetWord, NULL, &pszSyslogMessageOID, STD_LOADABLE_MODULE_ID));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmpspecifictype", 0, eCmdHdlrInt, NULL, &iSpecificType, NULL));
	CHKiRet(regCfSysLineHdlr(	(uchar *)"actionsnmptraptype", 0, eCmdHdlrInt, NULL, &iTrapType, NULL));
	CHKiRet(omsdRegCFSLineHdlr(	(uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL, STD_LOADABLE_MODULE_ID));
ENDmodInit
/*
 * vi:set ai:
 */
