'''
   Copyright (C) 2012 STFC.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   
   @author: Kevin Haines, Will Rogers
   
   The crypto module calls openssl command line directly, using subprocess.
   We investigated python's crypto libraries (all openssl bindings) and 
   found that none were mature enough to implement the SMIME crypto we had 
   decided on.
'''

from subprocess import Popen, PIPE
import logging

# logging configuration
log = logging.getLogger(__name__)
# Valid ciphers
CIPHERS = ['aes128', 'aes192', 'aes256']

class CryptoException(Exception):
    '''
    Exception for use by the crypto module.
    '''
    pass


def _from_file(filename):
    '''
    Convenience function to read entire file into string.
    '''
    f = open(filename, 'r')
    s = f.read()
    f.close()
    return s


def check_cert_key(certpath, keypath):
    '''
    Check that a certificate and a key match, using openssl directly to fetch
    the modulus of each, which must be the same.
    '''
    try:
        cert = _from_file(certpath)
        key = _from_file(keypath)
    except IOError, e:
        raise CryptoException('Could not find cert or key file: %s' % e)
    
    # Two things the same have the same modulus.
    if cert == key:
        return False

    p1 = Popen(['openssl', 'x509', '-noout', '-modulus'],
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    modulus1, error = p1.communicate(cert)

    if error != '':
        log.error(error)
        return False

    p2 = Popen(['openssl', 'rsa', '-noout', '-modulus'],
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    modulus2, error = p2.communicate(key)

    if error != '':
        log.error(error)
        return False
    
    return modulus1.strip() == modulus2.strip()


def sign(text, certpath, keypath):
    '''
    Sign the specified message using the certificate and key in the files specified.
    
    Returns the signed message as an SMIME string, suitable for transmission.
    '''
    try:
        p1 = Popen(['openssl', 'smime', '-sign', '-inkey', keypath, '-signer', certpath, '-text'],
                   stdin=PIPE, stdout=PIPE, stderr=PIPE)
        
        signed_msg, error = p1.communicate(text)
        
        if (error != ''):
            log.error(error)

        return signed_msg
    except OSError, e:
        log.error('Failed to sign message: %s' % e)
        raise CryptoException('Message signing failed.  Check cert and key permissions.')


def encrypt(text, certpath, cipher='aes128'):
    '''
    Encrypt the specified message using the certificate string.
    
    Returns the encrypted SMIME text suitable for transmission
    '''
    if cipher not in CIPHERS:
        raise CryptoException('Invalid cipher %s.' % cipher)
    
    cipher = '-' + cipher
    # encrypt
    p1 = Popen(['openssl', 'smime', '-encrypt', cipher, certpath], 
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    
    enc_txt, error = p1.communicate(text)
    
    if (error != ''):
        log.error(error)

    return enc_txt

def verify(signed_text, capath, check_crl):
    '''
    Verify the signed message has been signed by the certificate (attached to the
    supplied SMIME message) it claims to have, by one of the accepted CAs in
    capath.

    Returns a tuple including the signer's certificate and the plain-text of the
    message if it has been verified
    ''' 
    if signed_text is None or capath is None:
        raise CryptoException('Invalid None argument to verify().')
    # This ensures that openssl knows that the string is finished.
    # It makes no difference if the signed message is correct, but 
    # prevents it from hanging in the case of an empty string.
    signed_text += '\n\n'
    
    signer = get_signer_cert(signed_text)
    
    if not verify_cert(signer, capath, check_crl):
        raise CryptoException('Unverified signer')
    
    # The -noverify flag removes the certificate verification.  The certificate 
    # is verified above; this check would also check that the certificate
    # is allowed to sign with SMIME, which host certificates sometimes aren't.
    p1 = Popen(['openssl', 'smime', '-verify', '-CApath', capath, '-noverify', '-text'], 
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    
    message, error = p1.communicate(signed_text)
    
    # Interesting problem here - we get a message 'Verification successful'
    # to standard error.  We don't want to log this as an error each time,
    # but we do want to see if there's a genuine error...
    log.info(str(error).strip())

    subj = get_certificate_subject(signer)
    return message, subj


def decrypt(encrypted_text, certpath, keypath):
    '''
    Decrypt the specified message using the certificate and key contained in the
    named PEM files. The capath should point to a directory holding all the
    CAs that we accept

    This decryption function can be used whether or not OpenSSL is used to
    encrypt the data
    '''
    # This ensures that openssl knows that the string is finished.
    # It makes no difference if the signed message is correct, but 
    # prevents it from hanging in the case of an empty string.
    encrypted_text += '\n\n'
    
    log.info('Decrypting message.')
    
    p1 = Popen(['openssl', 'smime', '-decrypt',  
                '-recip', certpath, '-inkey', keypath], 
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    
    enc_txt, error = p1.communicate(encrypted_text)
    
    if (error != ''):
        log.error(error)

    return enc_txt


def verify_cert(certstring, capath, check_crls=True):
    '''
    Verify that the certificate is signed by a CA whose certificate is stored in
    capath.      

    Note that I've had to compare strings in the output of openssl to check
    for verification, which may make this brittle.

    Returns True if the certificate is verified
    '''
    if certstring is None or capath is None:
        raise CryptoException('Invalid None argument to verify_cert().')
    
    if check_crls:
        p1 = Popen(['openssl', 'verify', '-CApath', capath, '-crl_check_all'], 
                   stdin=PIPE, stdout=PIPE, stderr=PIPE)
    else: 
        p1 = Popen(['openssl', 'verify', '-CApath', capath], 
                   stdin=PIPE, stdout=PIPE, stderr=PIPE)
         
    message, error = p1.communicate(certstring)
    
    # I think this is unlikely ever to happen
    if (error != ''):
        log.error(error)
        
    # There was a tricky problem here.  
    # 'openssl verify' returns 0 whatever happens, so we can't 
    # use the return code to determine whether the verification was 
    # successful.  
    # If it is successful, openssl prints 'OK'
    # If it fails, openssl prints 'error'
    # So:
    log.info('Certificate verification: ' + str(message).strip())
      
    return ('OK' in message and not 'error' in message)


def get_certificate_subject(certstring):
    '''
    Returns the certificate subject's DN, in legacy openssl format.
    '''
    p1 = Popen(['openssl', 'x509', '-noout', '-subject'],
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    subject, error = p1.communicate(certstring)

    if (error != ''):
        log.error(error)
        raise CryptoException('Failed to get subject: %s' % error)
    
    subject = subject[9:] # remove 'subject= ' from the front
    subject = subject.strip()
    return subject

def get_signer_cert(signed_text):
    '''
    Read the signer's certificate from the specified message, and return the
    certificate string.

    Returns an X509 object for the signer's certificate
    '''
    # This ensures that openssl knows that the string is finished.
    # It makes no difference if the signed message is correct, but 
    # prevents it from hanging in the case of an empty string.
    signed_text += '\n\n'
    
    p1 = Popen(['openssl', 'smime', '-pk7out'], 
               stdin=PIPE, stdout=PIPE, stderr=PIPE)
    p2 = Popen(['openssl', 'pkcs7', '-print_certs'], 
               stdin=p1.stdout, stdout=PIPE, stderr=PIPE)
    
    p1.stdin.write(signed_text)
    certstring, error = p2.communicate()
    
    if (error != ''):
        log.error(error)
        
    return certstring

if __name__ == '__main__':
    
    CERTPATH = '/etc/grid-security/hostcert.pem'
    KEYPATH = '/etc/grid-security/hostkey.pem'
    
    MESSAGE = ('test')
    
    CERTSTRING = _from_file(CERTPATH)
    print get_certificate_subject(CERTSTRING)
    
    ENC = encrypt(MESSAGE, CERTPATH, 'aes256')
    print ENC
    DEC = decrypt(ENC, CERTPATH, KEYPATH)
    print DEC
    
    
