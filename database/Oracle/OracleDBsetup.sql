/*
 * A minimalist IDC Oracle database schema with only those tables
 * that iLoc uses.
/*
 * create lastid table
 */
create table lastid (
    KEYNAME     VARCHAR2(15) NOT NULL,
    KEYVALUE    NUMBER(10) NOT NULL,
    LDDATE      DATE
);
insert into lastid values ('ampid',0,Sysdate);
insert into lastid values ('arid',0,Sysdate);
insert into lastid values ('chanid',0,Sysdate);
insert into lastid values ('commid',0,Sysdate);
insert into lastid values ('dataid',0,Sysdate);
insert into lastid values ('dervid',0,Sysdate);
insert into lastid values ('dfid',0,Sysdate);
insert into lastid values ('dlid',0,Sysdate);
insert into lastid values ('dqwfid',0,Sysdate);
insert into lastid values ('evid',0,Sysdate);
insert into lastid values ('fkid',0,Sysdate);
insert into lastid values ('fpid',0,Sysdate);
insert into lastid values ('frid',0,Sysdate);
insert into lastid values ('glid',0,Sysdate);
insert into lastid values ('fkid',0,Sysdate);
insert into lastid values ('hydro_id',0,Sysdate);
insert into lastid values ('inid',0,Sysdate);
insert into lastid values ('intvlid',0,Sysdate);
insert into lastid values ('magid',0,Sysdate);
insert into lastid values ('msgdid',0,Sysdate);
insert into lastid values ('msgid',0,Sysdate);
insert into lastid values ('orid',2,Sysdate);
insert into lastid values ('otgid',2,Sysdate);
insert into lastid values ('parid',0,Sysdate);
insert into lastid values ('plid',0,Sysdate);
insert into lastid values ('pmccrecid',0,Sysdate);
insert into lastid values ('prid',0,Sysdate);
insert into lastid values ('prodid',0,Sysdate);
insert into lastid values ('qcstatsid',0,Sysdate);
insert into lastid values ('qualid',0,Sysdate);
insert into lastid values ('reqid',0,Sysdate);
insert into lastid values ('revid',0,Sysdate);
insert into lastid values ('stassid',0,Sysdate);
insert into lastid values ('subsid',0,Sysdate);
insert into lastid values ('tmp$object',0,Sysdate);
insert into lastid values ('userid',0,Sysdate);
insert into lastid values ('wfid',0,Sysdate);
insert into lastid values ('wfpid',0,Sysdate);
insert into lastid values ('wintid',0,Sysdate);
insert into lastid values ('wmcid',0,Sysdate);
insert into lastid values ('wmid',0,Sysdate);
insert into lastid values ('wmrid',0,Sysdate);


/*
 * create affiliation table
 */
create table affiliation (
    NET     VARCHAR2(8) NOT NULL,
    STA     VARCHAR2(6) NOT NULL,
    LDDATE  DATE,
CONSTRAINT pk_affiliation PRIMARY KEY (NET, STA)
);
create unique index affstax on affiliation (sta, net);
create index affnetx on affiliation (net);

/*
 * create site table
 */
create table site (
    STA     VARCHAR2(6) NOT NULL,
    ONDATE  NUMBER(8) NOT NULL,
    OFFDATE NUMBER(8),
    LAT     FLOAT(24),
    LON     FLOAT(24),
    ELEV    FLOAT(24),
    STANAME VARCHAR2(50),
    STATYPE VARCHAR2(4),
    REFSTA  VARCHAR2(6),
    DNORTH  FLOAT(24),
    DEAST   FLOAT(24),
    LDDATE  DATE,
CONSTRAINT pk_site PRIMARY KEY (STA, ONDATE)
);

/*
 * create amplitude table
 */
create table amplitude (
    AMPID       NUMBER(10) NOT NULL,
    ARID        NUMBER(10),
    PARID       NUMBER(10),
    CHAN        VARCHAR2(8),
    AMP         FLOAT(24),
    PER         FLOAT(24),
    SNR         FLOAT(24),
    AMPTIME     FLOAT(53),
    START_TIME  FLOAT(53),
    DURATION    FLOAT(24),
    BANDW       FLOAT(24),
    AMPTYPE     VARCHAR2(8),
    UNITS       VARCHAR2(15),
    CLIP        VARCHAR2(1),
    INARRIVAL   VARCHAR2(1),
    AUTH        VARCHAR2(15),
    LDDATE      DATE,
CONSTRAINT pk_amplitude PRIMARY KEY (AMPID)
);
create index amparidx on amplitude (arid);

/*
 * create arrival table
 */
create table arrival (
	sta   	VARCHAR2(6)	NOT NULL,
	time  	FLOAT(53) NOT NULL,
	arid	NUMBER(10) NOT NULL,
	jdate	NUMBER(8),
	stassid	NUMBER(10),
	chanid	NUMBER(8),
	chan  	VARCHAR2(8),
	iphase	VARCHAR2(8),
	stype	VARCHAR2(1),
	deltim	FLOAT(24),
	azimuth	FLOAT(24),
	delaz	FLOAT(24),
	slow	FLOAT(24),
	delslo	FLOAT(24),
	ema	    FLOAT(24),
	rect	FLOAT(24),
	amp	    FLOAT(24),
	per	    FLOAT(24),
	logat	FLOAT(24),
	clip	VARCHAR2(1),
	fm	    VARCHAR2(2),
	snr	    FLOAT(24),
	qual	VARCHAR2(1),
	auth	VARCHAR2(15),
	commid	NUMBER(10),
	lddate	DATE,
CONSTRAINT pk_arrival PRIMARY KEY (arid)
);
create index arrtimex on arrival (time);
create index arrstax on arrival (sta);

/*
 * create assoc table
 */
create table assoc (
    arid	NUMBER(10) NOT NULL,
    orid	NUMBER(10) NOT NULL,
    sta	    VARCHAR2(6),
    phase	VARCHAR2(8),
    belief	FLOAT(24),
    delta	FLOAT(24),
    seaz	FLOAT(24),
    esaz	FLOAT(24),
    timeres	FLOAT(24),
    timedef VARCHAR2(1),
    azres	FLOAT(24),
    azdef	VARCHAR2(1),
    slores	FLOAT(24),
    slodef	VARCHAR2(1),
    emares	FLOAT(24),
    wgt	    FLOAT(24),
    vmodel	VARCHAR2(15),
    commid	NUMBER(10),
    lddate	DATE,
CONSTRAINT PK_assoc PRIMARY KEY (orid, arid)
);

/*
 * create origin table
 */
create table origin (
	lat 	FLOAT(24) NOT NULL,
	lon	    FLOAT(24) NOT NULL,
	depth	FLOAT(24) NOT NULL,
	time	FLOAT(53) NOT NULL,
	orid	NUMBER(10) NOT NULL,
	evid	NUMBER(10),
	jdate	NUMBER(8),
	nass	NUMBER(4),
	ndef	NUMBER(4),
	ndp	    NUMBER(4),
	grn	    NUMBER(8),
	srn	    NUMBER(8),
	etype	VARCHAR2(7),
	depdp	FLOAT(24),
	dtype	VARCHAR2(1),
	mb	    FLOAT(24),
	mbid	NUMBER(10),
	ms	    FLOAT(24),
	msid	NUMBER(10),
	ml	    FLOAT(24),
	mlid	NUMBER(10),
	algorithm	VARCHAR2(15),
	auth	VARCHAR2(15),
	commid	NUMBER(10),
	lddate	DATE,
CONSTRAINT pk_origin PRIMARY KEY (orid)
);
create index origtimex on origin (time);
CREATE INDEX origlalon on origin (LAT,LON);
CREATE INDEX origevidx on origin (evid);

/*
 * create origerr table
 */
create table origerr (
	orid	NUMBER(10) NOT NULL,
	sxx	    FLOAT(24),
	syy	    FLOAT(24),
	szz	    FLOAT(24),
	stt	    FLOAT(24),
	sxy	    FLOAT(24),
	sxz	    FLOAT(24),
	syz	    FLOAT(24),
	stx	    FLOAT(24),
	sty	    FLOAT(24),
	stz	    FLOAT(24),
	sdobs	FLOAT(24),
	smajax	FLOAT(24),
	sminax	FLOAT(24),
	strike	FLOAT(24),
	sdepth	FLOAT(24),
	stime	FLOAT(24),
	conf	FLOAT(24),
	commid	NUMBER(10),
	lddate	DATE,
CONSTRAINT pk_origerr PRIMARY KEY (orid)
);

/*
 * create netmag table
 */
create table netmag (
	magid		NUMBER(10) NOT NULL,
	net		    VARCHAR2(8),
	orid		NUMBER(10) NOT NULL,
	evid		NUMBER(10),
	magtype		VARCHAR2(6),
	nsta		NUMBER(8),
	magnitude	FLOAT(24),
	uncertainty	FLOAT(24),
	auth		VARCHAR2(15),
	commid		NUMBER(10),
	lddate		DATE,
CONSTRAINT pk_netmag PRIMARY KEY (magid)
);
create index netmoridx on netmag (orid);

/*
 * create stamag table
 */
create table stamag (
	magid		NUMBER(10)	NOT NULL,
	ampid		NUMBER(10),
	sta		    VARCHAR2(6)	NOT NULL,
	arid		NUMBER(10),
	orid		NUMBER(10),
	evid		NUMBER(10),
	phase		VARCHAR2(8),
	delta		FLOAT(24),
	magtype		VARCHAR2(6),
	magnitude	FLOAT(24),
	uncertainty	FLOAT(24),
	magres		FLOAT(24),
	magdef		VARCHAR2(1),
	mmodel		VARCHAR2(15),
	auth		VARCHAR2(15),
	commid		NUMBER(10),
	lddate		date,
CONSTRAINT pk_stamag PRIMARY KEY (magid, sta)
);
create index smoridx on stamag (orid);
create index smaridx on stamag (arid);


