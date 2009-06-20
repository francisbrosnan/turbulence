/**
 * C inline representation for DTD turbulence-config.dtd, created by axl-knife
 */
#ifndef __TURBULENCE_CONFIG_DTD_H__
#define __TURBULENCE_CONFIG_DTD_H__
#define TURBULENCE_CONFIG_DTD "\n\
<!-- main configuration setting -->                                                       \
<!ELEMENT turbulence (global-settings, modules, features?, profile-path-configuration)>   \
                                                                                          \
<!-- global-settings -->                                                                  \
<!ELEMENT global-settings (ports,                                                         \
                           listener,                                                      \
                           log-reporting,                                                 \
      tls-support,                                                                        \
      on-bad-signal,                                                                      \
      clean-start?,                                                                       \
      connections)>                                                                       \
                                                                                          \
<!ELEMENT ports           (port+)>                                                        \
<!ELEMENT port            (#PCDATA)>                                                      \
                                                                                          \
<!ELEMENT log-reporting (general-log, error-log, access-log, vortex-log) >                \
<!ATTLIST log-reporting enabled (yes|no) #REQUIRED>                                       \
                                                                                          \
<!ELEMENT general-log        EMPTY>                                                       \
<!ATTLIST general-log file   CDATA #REQUIRED>                                             \
                                                                                          \
<!ELEMENT error-log          EMPTY>                                                       \
<!ATTLIST error-log file     CDATA #REQUIRED>                                             \
                                                                                          \
<!ELEMENT access-log         EMPTY>                                                       \
<!ATTLIST access-log  file   CDATA #REQUIRED>                                             \
                                                                                          \
<!ELEMENT vortex-log         EMPTY>                                                       \
<!ATTLIST vortex-log  file   CDATA #REQUIRED>                                             \
                                                                                          \
<!ELEMENT tls-support       EMPTY>                                                        \
<!ATTLIST tls-support enabled (yes|no) #REQUIRED>                                         \
                                                                                          \
<!ELEMENT on-bad-signal       EMPTY>                                                      \
<!ATTLIST on-bad-signal action (hold|ignore|quit|exit) #REQUIRED>                         \
                                                                                          \
<!ELEMENT clean-start       EMPTY>                                                        \
<!ATTLIST clean-start   value  (yes|no) #REQUIRED>                                        \
<!ATTLIST on-bad-signal action (yes|no) #REQUIRED>                                        \
                                                                                          \
<!ELEMENT connections       (max-connections)>                                            \
<!ELEMENT max-connections   EMPTY>                                                        \
<!ATTLIST max-connections   hard-limit CDATA #REQUIRED                                    \
                            soft-limit CDATA #REQUIRED>                                   \
                                                                                          \
                                                                                          \
<!-- modules -->                                                                          \
<!ELEMENT modules        (directory+)>                                                    \
                                                                                          \
<!ELEMENT directory       EMPTY>                                                          \
<!ATTLIST directory src   CDATA #REQUIRED>                                                \
                                                                                          \
<!-- features -->                                                                         \
<!ELEMENT features       (request-x-client-close?)>                                       \
                                                                                          \
<!ELEMENT request-x-client-close       EMPTY>                                             \
<!ATTLIST request-x-client-close value (yes|no) #REQUIRED>                                \
                                                                                          \
<!-- listener to be started -->                                                           \
<!ELEMENT listener    (name+)>                                                            \
<!ELEMENT name        (#PCDATA)>                                                          \
                                                                                          \
<!-- profile-path-configuration support -->                                               \
<!ELEMENT profile-path-configuration  (path-def+)>                                        \
<!ELEMENT path-def        (if-success | allow)*>                                          \
                                                                                          \
<!ELEMENT if-success      (if-success | allow)*>                                          \
<!ELEMENT allow           (if-success | allow)*>                                          \
                                                                                          \
<!ATTLIST path-def                                                                        \
          path-name      CDATA #IMPLIED                                                   \
          server-name    CDATA #IMPLIED                                                   \
   src            CDATA #IMPLIED >                                                        \
                                                                                          \
<!ATTLIST if-success                                                                      \
          profile      CDATA #REQUIRED                                                    \
          connmark     CDATA #IMPLIED                                                     \
   max-per-conn CDATA #IMPLIED                                                            \
          preconnmark  CDATA #IMPLIED >                                                   \
                                                                                          \
<!ATTLIST allow                                                                           \
          profile      CDATA #REQUIRED                                                    \
   max-per-conn CDATA #IMPLIED                                                            \
          preconnmark  CDATA #IMPLIED >                                                   \
                                                                                          \
\n"
#endif